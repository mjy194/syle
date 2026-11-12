#include "nested_atomic_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
static _Alignas(4096) uint64_t table[2][512];
struct fixture {struct nested_root_catalog c;struct nrc_span spans[2];struct nrc_root roots[2];struct ad_updater_gate g;};
static const struct nrc_identity id={1,2,3,4},next={1,2,7,8};
static const struct ad_entry_stamp stamp={1,2,3,5,4},newstamp={1,2,7,6,8};
static bool authority(void *p,const struct nrc_span *s,uint64_t a,struct nrc_atomic_page *out)
{(void)p;(void)s;(void)a;(void)out;return false;}
static bool guard(void *p,struct nac_cache_identity c){(void)p;(void)c;return true;}
static void publish(struct fixture *f,struct nrc_identity identity) {
 for(unsigned i=0;i<2;i++) {
 uint64_t gpa=0x1000+0x2000*i;
 struct nrc_span s={.identity=identity,.object=1+i,.lifetime=2+i,.gpa=gpa,.hpa=gpa+0x1000,.bytes=4096,.table_view=table[i],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
 assert(nrc_add_atomic_span(&f->c,s,table[i]));
 assert(nrc_add_root(&f->c,(struct nrc_root){identity,1+i,0,1,1,gpa|0x5e,gpa}));
 }
 assert(nrc_publish(&f->c));
}
static void setup(struct fixture *f) {
 memset(f,0,sizeof *f);
 assert(nrc_init_atomic_wb(&f->c,id,48,f->spans,2,f->roots,2,authority));publish(f,id);
 assert(ad_updater_gate_init(&f->g,stamp));
 assert(!f->c.predecessor&&!f->g.predecessor);
}
static bool successor(struct fixture *n,struct fixture *o,uint64_t reserve) {
 return nrc_init_atomic_wb_successor(&n->c,&o->c,id,next,48,n->spans,2,n->roots,2,authority,reserve);
}
static void close_old(struct fixture *o) {
 assert(nrc_close(&o->c,id));assert(ad_updater_gate_close(&o->g,stamp));
}
static bool begin(struct fixture *f,struct nac_permit *p,struct nac_ticket *t) {
 return nac_begin(&f->c,f->roots[0],NULL,&f->g,f->g.stamp,
  (struct nac_cache_identity){0,0x9000,0x905e,1,1},guard,NULL,p,t);
}
/* Deliberately no counter offsets or new-counter assertions before the stale
 * guard. Omitting both successor seeds must reproduce the real {1,1} ABA. */
static void nac_double_nonce_collision(void) {
 struct fixture o,n={0};setup(&o);
 struct nac_permit p={0};struct nac_ticket oldticket,t;
 assert(begin(&o,&p,&oldticket));
 assert(oldticket.source_nonce==1&&oldticket.updater_nonce==1);
 assert(nac_end(&p,oldticket));close_old(&o);
 assert(successor(&n,&o,5));
 assert(ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,5));
 publish(&n,next);assert(begin(&n,&p,&t));
 assert(!nac_guard(&p,oldticket));
 assert(!nac_end(&p,oldticket));
 assert(nac_guard(&p,t)&&p.source.live&&p.updater.live);
 assert(nac_end(&p,t));
 puts("PASS N47A production NAC double-number collision rejected before highwater assertions");
}
static void same_storage(void) {
 struct fixture o,n={0},fork={0};setup(&o);
 /* Independently exercise the source counter; updater highwater differs. */
 struct nrc_session s={0};uint64_t nonce;
 for(int i=0;i<3;i++){assert(nrc_begin(&o.c,o.roots[0],NULL,&s,&nonce));assert(nrc_end(&s,nonce));}
 struct nac_permit p={0};struct nac_ticket oldticket,t;
 assert(begin(&o,&p,&oldticket));assert(oldticket.source_nonce==4&&oldticket.updater_nonce==1);
 close_old(&o);
 assert(!successor(&n,&o,5));assert(!ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,5));
 assert(nac_guard(&p,oldticket));assert(nac_end(&p,oldticket));
 assert(successor(&n,&o,5));assert(n.c.next_nonce==4);
 assert(ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,5));assert(n.g.next_nonce==1);
 assert(n.c.predecessor==&o.c&&n.g.predecessor==&o.g);
 assert(!fork.c.predecessor&&!fork.g.predecessor);
 assert(!successor(&fork,&o,1));assert(!ad_updater_gate_init_successor(&fork.g,&o.g,stamp,newstamp,1));
 struct ad_entry_stamp out={0};assert(!ad_updater_gate_reopen(&o.g,stamp,true,&out));
 assert(o.c.closed&&o.c.superseded&&o.g.closed&&o.g.superseded);
 publish(&n,next);assert(begin(&n,&p,&t));assert(t.source_nonce==5&&t.updater_nonce==2);
 assert(!nac_guard(&p,oldticket));assert(!nac_end(&p,oldticket));
 assert(!nrc_guard(&p.source,oldticket.source_nonce));assert(!nrc_end(&p.source,oldticket.source_nonce));
 assert(!ad_updater_gate_release(&n.g,&p.updater,newstamp,oldticket.updater_nonce));
 assert(!ad_updater_gate_release(&o.g,&p.updater,stamp,oldticket.updater_nonce));
 assert(nac_guard(&p,t)&&p.source.live&&p.updater.live&&n.c.references==1&&n.g.references==1);
 assert(nac_end(&p,t));puts("PASS N47A same production NAC permit storage rejects stale tickets and independent nonces");
}
static void rejection(void) {
 struct fixture o,n={0};setup(&o);
 assert(!successor(&n,&o,1));assert(!ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,1));close_old(&o);
 assert(!successor(&n,&o,0));assert(!ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,0));
 struct nrc_identity wrong=id;wrong.lifetime++;
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,wrong,next,48,n.spans,2,n.roots,2,authority,1));
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,id,id,48,n.spans,2,n.roots,2,authority,1));
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,id,next,48,n.spans,1,n.roots,2,authority,1));
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,id,next,48,n.spans,2,n.roots,1,authority,1));
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,id,next,48,n.spans,0,n.roots,2,authority,1));
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,id,next,48,n.spans,2,n.roots,0,authority,1));
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,id,next,48,n.spans,SIZE_MAX,n.roots,2,authority,1));
 assert(!nrc_init_atomic_wb_successor(&n.c,&o.c,id,next,48,o.spans,2,n.roots,2,authority,1));
 struct ad_entry_stamp bad=stamp;bad.epoch++;
 assert(!ad_updater_gate_init_successor(&n.g,&o.g,bad,newstamp,1));
 assert(!ad_updater_gate_init_successor(&n.g,&o.g,stamp,stamp,1));
 assert(!n.c.initialized&&!n.g.initialized&&!o.c.superseded&&!o.g.superseded);
 /* Synthetic highwater, exclusively before any concurrency: boundary budget. */
 o.c.next_nonce=UINT64_MAX-2;o.g.next_nonce=UINT64_MAX-3;
 assert(!successor(&n,&o,3));assert(!ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,4));
 assert(successor(&n,&o,2));assert(ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,3));
 publish(&n,next);struct nac_permit p={0};struct nac_ticket t;
 assert(begin(&n,&p,&t));assert(t.source_nonce==UINT64_MAX-1&&t.updater_nonce==UINT64_MAX-2);assert(nac_end(&p,t));
 assert(begin(&n,&p,&t));assert(t.source_nonce==UINT64_MAX&&t.updater_nonce==UINT64_MAX-1);assert(nac_end(&p,t));
 assert(!begin(&n,&p,&t));
 puts("PASS N47A preconditions capacities identity budget overflow and atomic rejection");
}
struct race {struct fixture *old;struct fixture fresh;bool catalog,gate;};
static void *race_run(void *v){struct race *r=v;r->catalog=successor(&r->fresh,r->old,1);r->gate=ad_updater_gate_init_successor(&r->fresh.g,&r->old->g,stamp,newstamp,1);return NULL;}
static void concurrent_fork(void) {
 struct fixture o;setup(&o);close_old(&o);struct race a={.old=&o},b={.old=&o};pthread_t x,y;
 assert(!pthread_create(&x,NULL,race_run,&a));assert(!pthread_create(&y,NULL,race_run,&b));
 assert(!pthread_join(x,NULL));assert(!pthread_join(y,NULL));assert(a.catalog!=b.catalog&&a.gate!=b.gate);
 puts("PASS N47A concurrent fork has exactly one winner per independent gate");
}
static void poisoned_cleanup(void) {
 struct fixture o,n={0};setup(&o);struct nac_permit p={0};struct nac_ticket t;assert(begin(&o,&p,&t));close_old(&o);
 ad_updater_gate_poison(&o.g);__atomic_store_n(&o.c.fault,1,__ATOMIC_RELEASE);
 assert(!nac_guard(&p,t));assert(nac_end(&p,t));assert(!o.c.references&&!o.g.references);
 assert(!successor(&n,&o,1));assert(!ad_updater_gate_init_successor(&n.g,&o.g,stamp,newstamp,1));
 puts("PASS N47A poison preserves original-ticket cleanup and rejects successor");
}
int main(void){nac_double_nonce_collision();same_storage();rejection();concurrent_fork();poisoned_cleanup();return 0;}
