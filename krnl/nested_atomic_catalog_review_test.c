/* Independent N41A source/permit/trace checks against production APIs. */
#define _GNU_SOURCE
#include "nested_atomic_catalog.h"
#include "nested_ept_sparse.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Alignas(4096) uint64_t tables[9][512];
struct fixture {
 struct nested_root_catalog catalog;
 struct nrc_span spans[3];struct nrc_root roots[3];
 struct nrc_identity id;struct ad_entry_stamp stamp;struct ad_updater_gate gate;
};
struct owner {
 struct fixture *fixture;struct nac_cache_identity cache;
 unsigned authority_calls,cache_calls,corrupt,deny_cache_call;
 bool deny_data,cache_bad,close_source,close_updater,concurrent;
 struct nested_ept_sparse *actual_tree;
};
static bool authority(void *p,const struct nrc_span *span,uint64_t gpa,struct nrc_atomic_page *out) {
 struct owner *o=p;struct fixture *f=o->fixture;o->authority_calls++;
 if(!o->concurrent)assert(!__atomic_load_n(&f->catalog.lock,__ATOMIC_ACQUIRE));
 if(o->deny_data&&span->role==NRC_DATA)return false;
 uint64_t offset=gpa-span->gpa;
 uint64_t *view=span->role==NRC_TABLE?&tables[0][0]+offset/8:NULL;
 *out=(struct nrc_atomic_page){.backing={.object=span->object,.lifetime=span->lifetime,
  .hpa=span->hpa+offset,.table_view=view,.permissions=span->permissions,
  .memory_type=6,.ignore_pat=true},.atomic_view=view,.pinned_writable=view!=NULL};
 switch(o->corrupt){
 case 1:out->backing.object++;break;case 2:out->backing.lifetime++;break;
 case 3:out->backing.hpa+=4096;break;case 4:out->backing.table_view=NULL;break;
 case 5:out->backing.permissions^=2;break;case 6:out->backing.memory_type=0;break;
 case 7:out->backing.memory_type=4;break;case 8:out->backing.ignore_pat=false;break;
 case 9:out->atomic_view=NULL;break;case 10:out->pinned_writable=false;break;
 case 11:out->atomic_view=tables[8];out->pinned_writable=true;break;
 }
 if(o->close_source){o->close_source=false;assert(nrc_close(&f->catalog,f->id));}
 if(o->close_updater){o->close_updater=false;assert(ad_updater_gate_close(&f->gate,f->stamp));}
 return true;
}
static bool cache_guard(void *p,struct nac_cache_identity c) {
 struct owner *o=p;o->cache_calls++;
 return !o->cache_bad&&o->cache_calls!=o->deny_cache_call&&
  (!o->actual_tree||!o->actual_tree->poisoned)&&!memcmp(&c,&o->cache,sizeof(c));
}
static void init(struct fixture *f) {
 memset(f,0,sizeof(*f));memset(tables,0,sizeof(tables));
 f->id=(struct nrc_identity){21,22,23,24};
 f->stamp=(struct ad_entry_stamp){21,22,23,25,24};
 assert(nrc_init_atomic_wb(&f->catalog,f->id,48,f->spans,3,f->roots,3,authority));
 struct nrc_span span={.identity=f->id,.object=31,.lifetime=32,.gpa=0x1000,
  .hpa=0x100000,.bytes=sizeof(tables),.table_view=tables[0],.permissions=1,
  .allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
 assert(!nrc_add_span(&f->catalog,span));assert(!nrc_add_atomic_span(&f->catalog,span,NULL));
 assert(nrc_add_atomic_span(&f->catalog,span,tables[0]));
 span=(struct nrc_span){.identity=f->id,.object=41,.lifetime=42,.gpa=0x90000,
  .hpa=0x900000,.bytes=16384,.permissions=3,.allowed_types=NRC_TYPE_WB,
  .role=NRC_DATA,.ignore_pat=true};
 struct nrc_span bad=span;bad.allowed_types|=NRC_TYPE_UC;
 assert(!nrc_add_atomic_span(&f->catalog,bad,NULL));
 bad=span;bad.allowed_types=1u<<4;assert(!nrc_add_atomic_span(&f->catalog,bad,NULL));
 assert(!nrc_add_atomic_span(&f->catalog,span,tables[0]));
 assert(nrc_add_atomic_span(&f->catalog,span,NULL));
 const uint64_t roots[]={0x1000,0x5000,0x6000};
 for(unsigned i=0;i<3;i++){
  struct nrc_root r={.identity=f->id,.source=51+i,.owner=61+i,.generation=71+i,
   .lifetime=81+i,.full_eptp=roots[i]|0x5e,.root_gpa=roots[i]};
  struct nrc_root badroot=r;badroot.full_eptp^=0x40;
  assert(!nrc_add_root(&f->catalog,badroot));assert(nrc_add_root(&f->catalog,r));
 }
 tables[0][0]=0x2007;tables[1][0]=0x3007;tables[2][0]=0x4007;tables[3][15]=0x90073;
 tables[4][0]=0x2007;
 tables[5][0]=0x7007;tables[6][0]=0x8007;tables[7][0]=0x9007;tables[8][15]=0x91073;
 assert(nrc_publish(&f->catalog));assert(ad_updater_gate_init(&f->gate,f->stamp));
}
static struct owner owner(struct fixture *f,unsigned index) {
 return (struct owner){.fixture=f,.cache={.owner=index,.hpa=0xa00000+index*4096,
  .full_eptp=(0xa00000+index*4096)|0x5e,.lifetime=91+index,.generation=101+index}};
}
static struct nac_ticket begin(struct owner *o,unsigned root,struct nac_permit *p) {
 struct fixture *f=o->fixture;struct nac_ticket t={0};
 assert(nac_begin(&f->catalog,f->roots[root],o,&f->gate,f->stamp,o->cache,cache_guard,o,p,&t));
 assert(t.source_nonce&&t.updater_nonce);return t;
}
static struct nac_trace read_trace(struct nac_permit *p,struct nac_ticket t) {
 struct nac_trace trace;struct nested_ept_atomic_result r=nac_walk(p,t,0xf037,1,&trace);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&trace.complete);
 assert(nac_trace_guard(p,t,0xf037,&trace));return trace;
}
static void shared_and_terminal(void) {
 struct fixture f;init(&f);struct owner a=owner(&f,0),b=owner(&f,1),c=owner(&f,2);
 struct nac_permit ap={0},bp={0},cp={0};
 struct nac_ticket at=begin(&a,0,&ap),bt=begin(&b,1,&bp),ct=begin(&c,2,&cp);
 struct nrc_page ordinary={.object=0xabc},saved=ordinary;
 assert(!nrc_resolve(&ap.source,at.source_nonce,0x1000,1,&ordinary));
 assert(!memcmp(&ordinary,&saved,sizeof(saved)));
 assert(nrc_walk(&ap.source,at.source_nonce,0xf000,1).status==NESTED_EPT_UNSUPPORTED);
 assert(!a.authority_calls);struct nac_trace ar=read_trace(&ap,at),br;
 assert(ar.translation.hpa==0x900037);
 struct nested_ept_atomic_result r=nac_walk(&bp,bt,0xf037,2,&br);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&br.complete);
 assert(ar.path.entry[1].pointer==br.path.entry[1].pointer);
 assert(ar.path.entry[3].pointer==br.path.entry[3].pointer);
 assert(nac_trace_guard(&ap,at,0xf037,&ar)); /* Peer D is a permitted change. */
 struct nac_trace cr=read_trace(&cp,ct);assert(cr.translation.hpa==0x901037);
 assert(cr.path.entry[3].pointer!=ar.path.entry[3].pointer);
 assert((__atomic_load_n(&tables[3][15],__ATOMIC_ACQUIRE)&0x300)==0x300);
 assert((__atomic_load_n(&tables[8][15],__ATOMIC_ACQUIRE)&0x300)==0x100);
 assert(f.catalog.references==3&&f.gate.references==3);
 assert(nrc_close(&f.catalog,f.id)&&ad_updater_gate_close(&f.gate,f.stamp));
 assert(!nrc_drained(&f.catalog,f.id)&&!ad_updater_gate_drained(&f.gate,f.stamp));
 assert(nac_guard(&ap,at)&&nac_guard(&bp,bt)&&nac_guard(&cp,ct));
 assert(nac_complete(&ap,at)&&nac_complete(&bp,bt)&&nac_complete(&cp,ct));
 assert(nrc_drained(&f.catalog,f.id)&&ad_updater_gate_drained(&f.gate,f.stamp));
 struct ad_entry_stamp next;assert(ad_updater_gate_reopen(&f.gate,f.stamp,true,&next));
 struct nac_ticket out={0};
 assert(!nac_begin(&f.catalog,f.roots[0],&a,&f.gate,next,a.cache,cache_guard,&a,&ap,&out));
 assert(!nrc_publish(&f.catalog)); /* Updater reopening does not reopen immutable source. */
 puts("PASS review shared and distinct multi-root paths, dual lifetime and terminal catalog close");
}
static void authority_boundaries(void) {
 for(unsigned corruption=1;corruption<=11;corruption++){
  struct fixture f;init(&f);struct owner o=owner(&f,0);struct nac_permit p={0};
  struct nac_ticket t=begin(&o,0,&p);o.corrupt=corruption;
  struct nrc_atomic_page result={.backing={.object=0xabc}},saved=result;
  assert(!nrc_resolve_atomic(&p.source,t.source_nonce,0x1000,1,&result));
  assert(!memcmp(&saved,&result,sizeof(result)));
  struct nac_trace trace;struct nested_ept_atomic_result r=nac_walk(&p,t,0xf037,1,&trace);
  assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed&&!trace.complete);
  assert(nac_complete(&p,t));
 }
 struct fixture f;init(&f);struct owner o=owner(&f,0);struct nac_permit p={0};
 struct nac_ticket t=begin(&o,0,&p);o.corrupt=11;
 struct nrc_atomic_page result;
 assert(!nrc_resolve_atomic(&p.source,t.source_nonce,0x90000,1,&result));
 o.corrupt=0;o.deny_data=true;tables[3][15]&=~UINT64_C(2);
 struct nac_trace trace;struct nested_ept_atomic_result r=nac_walk(&p,t,0xf037,2,&trace);
 assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);
 o.deny_data=false;o.close_source=true;o.close_updater=true;
 r=nac_walk(&p,t,0xf037,1,&trace);assert(r.status==NEA_DONE&&trace.complete);
 assert(nac_complete(&p,t));
 puts("PASS review full atomic authority, output preservation, L0 precedence and admitted close");
}
static void durable_identity(void) {
 struct fixture f;init(&f);struct owner o=owner(&f,0);struct nac_permit p={0};
 struct nac_ticket t=begin(&o,0,&p);struct nac_trace original=read_trace(&p,t),bad;
 #define FIELD(x) do{bad=original;bad.x++;o.authority_calls=0;assert(!nac_trace_guard(&p,t,0xf037,&bad));assert(!o.authority_calls);}while(0)
 FIELD(source.identity.domain);FIELD(source.identity.incarnation);FIELD(source.identity.catalog);
 FIELD(source.identity.lifetime);FIELD(source.source);FIELD(source.owner);FIELD(source.generation);
 FIELD(source.lifetime);FIELD(source.full_eptp);FIELD(source.root_gpa);
 FIELD(cache.owner);FIELD(cache.hpa);FIELD(cache.full_eptp);FIELD(cache.lifetime);FIELD(cache.generation);
 FIELD(path.stamp.domain_id);FIELD(path.stamp.incarnation);FIELD(path.stamp.context_id);
 FIELD(path.stamp.epoch);FIELD(path.stamp.lifetime);FIELD(gpa);
 #undef FIELD
 assert(!nac_trace_guard(&p,t,0xf000,&original));
 for(unsigned i=0;i<4;i++){
  bad=original;bad.path.entry[i].pointer=(void *)1;assert(!nac_trace_guard(&p,t,0xf037,&bad));
  bad=original;bad.path.entry[i].entry_gpa+=8;assert(!nac_trace_guard(&p,t,0xf037,&bad));
  bad=original;bad.path.entry[i].table_hpa+=4096;assert(!nac_trace_guard(&p,t,0xf037,&bad));
  bad=original;bad.path.entry[i].raw^=0x1000;assert(!nac_trace_guard(&p,t,0xf037,&bad));
  bad=original;bad.path.entry[i].leaf=!bad.path.entry[i].leaf;assert(!nac_trace_guard(&p,t,0xf037,&bad));
 }
 #define TRANSLATION(x) do{bad=original;bad.translation.x++;assert(!nac_trace_guard(&p,t,0xf037,&bad));}while(0)
 TRANSLATION(l1_gpa);TRANSLATION(hpa);TRANSLATION(effective12);TRANSLATION(effective02);
 TRANSLATION(leaf_type);TRANSLATION(walk_type);
 #undef TRANSLATION
 bad=original;bad.translation.ignore_pat=false;assert(!nac_trace_guard(&p,t,0xf037,&bad));
 bad=original;bad.translation.memory_explicit=false;assert(!nac_trace_guard(&p,t,0xf037,&bad));
 assert(nac_complete(&p,t));struct nac_ticket next=begin(&o,0,&p);
 assert(next.source_nonce!=t.source_nonce&&next.updater_nonce!=t.updater_nonce);
 assert(!nac_guard(&p,t)&&!nac_end(&p,t));assert(nac_trace_guard(&p,next,0xf037,&original));
 assert(nac_complete(&p,next));
 assert(ad_updater_gate_close(&f.gate,f.stamp));struct ad_entry_stamp newer;
 assert(ad_updater_gate_reopen(&f.gate,f.stamp,true,&newer));f.stamp=newer;
 next=begin(&o,0,&p);assert(!nac_trace_guard(&p,next,0xf037,&original));
 assert(nac_complete(&p,next));
 o.cache.lifetime++;next=begin(&o,0,&p);assert(!nac_trace_guard(&p,next,0xf037,&original));
 assert(nac_complete(&p,next));
 puts("PASS review complete source/cache/AD/GPA identity, corrupt paths and stale permit/trace ABA");
}
static void cleanup_retention(void) {
 for(unsigned which=0;which<2;which++){
  struct fixture f;init(&f);struct owner o=owner(&f,0);struct nac_permit p={0};
  struct nac_ticket t=begin(&o,0,&p);(void)read_trace(&p,t);
  if(which)f.catalog.lock=1;else f.gate.lock=1;
  assert(!nac_end(&p,t));assert(p.active&&p.source.live&&f.catalog.references==1);
  if(which)assert(!p.updater.live&&!f.gate.references);
  else assert(p.updater.live&&f.gate.references==1);
  assert(f.catalog.fault&&f.gate.fault);
  f.catalog.lock=0;f.gate.lock=0;
  assert(!nac_guard(&p,t));assert(nac_end(&p,t));
  assert(!p.active&&!p.source.live&&!p.updater.live&&!f.catalog.references&&!f.gate.references);
 }
 struct fixture f;init(&f);struct owner o=owner(&f,0);struct nac_permit p={0};
 assert(ad_updater_gate_close(&f.gate,f.stamp));struct nac_ticket t={0};
 assert(!nac_begin(&f.catalog,f.roots[0],&o,&f.gate,f.stamp,o.cache,cache_guard,&o,&p,&t));
 assert(t.source_nonce&&!t.updater_nonce&&!p.active&&!f.catalog.references);
 struct ad_entry_stamp next;assert(ad_updater_gate_reopen(&f.gate,f.stamp,true,&next));f.stamp=next;
 t=begin(&o,0,&p);(void)read_trace(&p,t);o.cache_bad=true;
 assert(!nac_complete(&p,t));assert(!p.active&&!f.catalog.references&&!f.gate.references);
 puts("PASS review updater-before-source cleanup, both release failures retained and final guard failure");
}
static void transient_cache_guards(void) {
 struct fixture f;init(&f);struct owner o=owner(&f,0);struct nac_permit p={0};
 struct nac_ticket t=begin(&o,0,&p);o.cache_calls=0;struct nac_trace trace;
 struct nested_ept_atomic_result r=nac_walk(&p,t,0xf037,2,&trace);
 assert(r.status==NEA_DONE);unsigned guards=o.cache_calls;assert(nac_complete(&p,t));
 bool partial=false;
 for(unsigned i=1;i<=guards;i++){
  init(&f);o=owner(&f,0);p=(struct nac_permit){0};t=begin(&o,0,&p);
  o.cache_calls=0;o.deny_cache_call=i;r=nac_walk(&p,t,0xf037,2,&trace);
  assert(r.status==NEA_STALE&&!trace.complete);partial|=r.changed;
  o.deny_cache_call=0;assert(nac_end(&p,t));
 }
 assert(partial);
 puts("PASS review every transient cache-guard failure preserves stale ownership, including partial CAS");
}
struct thread {struct owner owner;unsigned root;};
static void *worker(void *p) {
 struct thread *t=p;
 for(unsigned n=0;n<40;n++){
  struct nac_permit permit={0};struct nac_ticket ticket=begin(&t->owner,t->root,&permit);
  struct nac_trace trace;struct nested_ept_atomic_result r=nac_walk(&permit,ticket,0xf037,
   (t->owner.cache.owner&1)?2:1,&trace);
  assert(r.status==NEA_DONE&&trace.complete&&nac_trace_guard(&permit,ticket,0xf037,&trace));
  assert(nac_complete(&permit,ticket));
 }
 return NULL;
}
static void concurrent_catalog(void) {
 for(unsigned round=0;round<10;round++){
  struct fixture f;init(&f);struct thread t[6];pthread_t threads[6];
  for(unsigned i=0;i<6;i++){
   t[i]=(struct thread){.owner=owner(&f,i),.root=i%3};t[i].owner.concurrent=true;
   assert(!pthread_create(&threads[i],NULL,worker,&t[i]));
  }
  for(unsigned i=0;i<6;i++)assert(!pthread_join(threads[i],NULL));
  assert(!f.catalog.references&&!f.gate.references);
  assert(nrc_close(&f.catalog,f.id)&&nrc_drained(&f.catalog,f.id));
  assert(ad_updater_gate_close(&f.gate,f.stamp)&&ad_updater_gate_drained(&f.gate,f.stamp));
  assert((__atomic_load_n(&tables[3][15],__ATOMIC_ACQUIRE)&0x300)==0x300);
  assert((__atomic_load_n(&tables[8][15],__ATOMIC_ACQUIRE)&0x300)==0x300);
 }
 puts("PASS review 2,400 actual six-thread source/walk/trace operations over shared and distinct paths");
}
struct pool {
 struct owner *owner;struct nac_permit *permit;struct nac_ticket ticket;
 struct nested_sparse_page pages[32];unsigned allocated,freed,flushes;
 bool enforce,fail_flush;
};
static bool allocate(void *p,struct nested_sparse_page *page) {
 struct pool *pool=p;assert(pool->allocated<32);
 void *view=aligned_alloc(4096,4096);if(!view)return false;
 *page=(struct nested_sparse_page){.hpa=0x1000000+4096*pool->allocated,.view=view};
 pool->pages[pool->allocated++]=*page;return true;
}
static void free_page(void *p,struct nested_sparse_page page) {
 struct pool *pool=p;pool->freed++;free(page.view);
}
static bool flush(void *p,uint64_t eptp) {
 struct pool *pool=p;pool->flushes++;assert((eptp&4095)==0x5e);
 if(pool->enforce){
  assert(nac_guard(pool->permit,pool->ticket));
  assert(pool->owner->fixture->catalog.references==1&&pool->owner->fixture->gate.references==1);
 }
 return !pool->fail_flush;
}
static uint64_t physical_leaf(struct pool *p,uint64_t eptp,uint64_t gpa) {
 uint64_t next=eptp&~UINT64_C(4095),raw=0;
 for(unsigned level=0;level<4;level++){
  uint64_t *view=NULL;
  for(unsigned i=0;i<p->allocated;i++)if(p->pages[i].hpa==next)view=p->pages[i].view;
  assert(view);raw=view[(gpa>>(39-9*level))&511];next=raw&~UINT64_C(4095);
 }
 return raw;
}
static void sparse_publication(void) {
 for(unsigned failure=0;failure<2;failure++){
  struct fixture f;init(&f);struct owner o=owner(&f,0);struct nac_permit permit={0};
  struct nested_ept_sparse tree;struct pool pool={.owner=&o,.permit=&permit};
  const struct nested_sparse_ops ops={allocate,free_page,flush};
  assert(nested_ept_sparse_init_mode(&tree,&ops,&pool,48,true));o.actual_tree=&tree;
  o.cache.hpa=tree.eptp&~UINT64_C(4095);o.cache.full_eptp=tree.eptp;o.cache.generation=tree.generation;
  __atomic_fetch_or(&tables[3][15],UINT64_C(0x200),__ATOMIC_ACQ_REL);
  pool.ticket=begin(&o,0,&permit);pool.enforce=true;struct nac_trace read;
  struct nested_ept_atomic_result r=nac_walk(&permit,pool.ticket,0xf000,1,&read);
  assert(r.status==NEA_DONE&&read.complete);
  assert(nested_ept_sparse_fill_tracked(&tree,0xf000,&r.translation,true)==NESTED_SPARSE_OK);
  assert((physical_leaf(&pool,tree.eptp,0xf000)&7)==1); /* Even with peer D=1. */
  assert(nac_complete(&permit,pool.ticket));
  pool.ticket=begin(&o,0,&permit);
  assert(nac_trace_guard(&permit,pool.ticket,0xf000,&read));
  struct nac_trace write;r=nac_walk(&permit,pool.ticket,0xf000,2,&write);
  assert(r.status==NEA_DONE&&write.complete);pool.fail_flush=failure;
  unsigned freed=pool.freed;
  assert(nested_ept_sparse_promote_write(&tree,0xf000)==!failure);
  assert(pool.freed==freed&&tree.root&&tree.leaves);
  if(failure){assert(tree.poisoned);assert(!nac_complete(&permit,pool.ticket));}
  else {assert((physical_leaf(&pool,tree.eptp,0xf000)&7)==3);assert(nac_complete(&permit,pool.ticket));}
  pool.enforce=false;
  if(failure){assert(!nested_ept_sparse_destroy(&tree));assert(pool.freed==freed&&tree.root);}
  pool.fail_flush=false;assert(nested_ept_sparse_destroy(&tree));assert(pool.allocated==pool.freed);
 }
 puts("PASS review actual EPT02 RO despite peer D, held permits through publication/INVEPT, failed flush retention");
}
int main(int argc,char **argv) {
 const char *only=argc>1?argv[1]:"all";
 #define RUN(n) do{if(!strcmp(only,"all")||!strcmp(only,#n))n();}while(0)
 RUN(shared_and_terminal);RUN(authority_boundaries);RUN(durable_identity);RUN(cleanup_retention);RUN(transient_cache_guards);RUN(concurrent_catalog);RUN(sparse_publication);
 puts("PASS independent N41A atomic catalog review");
}
