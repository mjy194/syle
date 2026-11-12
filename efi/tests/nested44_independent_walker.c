#define _GNU_SOURCE
#include "nested_atomic_catalog.h"
#include "nested_ept_sparse.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BASE UINT64_C(0x800000)
#define L2 UINT64_C(0x400000)
#define AM UINT64_C(0x000ffffffffff000)
static _Alignas(4096) uint64_t pages[5][512];
static const struct ad_entry_stamp stamp={11,12,13,14,15};
struct fixture {unsigned guards,deny_guard,loads,compares,table_calls[5],leaf_calls;
 uint64_t requested; uint8_t permissions; bool deny_leaf,clear_dirty,structural,retry,wrong_hpa;
 pthread_barrier_t *barrier; unsigned *collisions;};
static uint64_t physical(uint64_t g){return UINT64_C(0x2000000)+(((g-BASE)>>12)^0x155)*4096;}
static bool guard(void *c,struct ad_entry_stamp s){struct fixture *f=c;assert(!memcmp(&s,&stamp,sizeof s));return ++f->guards!=f->deny_guard;}
static bool table(void *c,uint64_t g,uint64_t *h,uint64_t **v){struct fixture *f=c;
 if(g<0x1000||g>0x5000||(g&4095))return false;
 unsigned n=g/4096-1;f->table_calls[n]++;
 *v=pages[n];*h=0x100000+n*4096;if(f->wrong_hpa&&n==2&&f->table_calls[n]>1)*h+=4096;return true;}
static bool leaf(void *c,uint64_t g,uint64_t *h,uint8_t *p){struct fixture *f=c;f->leaf_calls++;f->requested=g;
 if(g<BASE||g>=BASE+0x200000||(g&4095))return false;
 *h=physical(g);*p=f->permissions;return !f->deny_leaf;}
static uint64_t load(void *c,uint64_t *p){struct fixture *f=c;f->loads++;
 if(f->clear_dirty&&f->loads==4)__atomic_fetch_and(&pages[2][2],~UINT64_C(0x200),__ATOMIC_ACQ_REL);
 return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static bool cas(void *c,uint64_t *p,uint64_t a,uint64_t b){struct fixture *f=c;f->compares++;
 if(f->retry)return false;
 if(f->structural){__atomic_fetch_xor(p,UINT64_C(0x80),__ATOMIC_ACQ_REL);return false;}
 if(f->barrier&&f->compares==1){assert(p==&pages[2][2]);int r=pthread_barrier_wait(f->barrier);assert(!r||r==PTHREAD_BARRIER_SERIAL_THREAD);}
 bool ok=__atomic_compare_exchange_n(p,&a,b,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 if(!ok&&f->collisions)__atomic_fetch_add(f->collisions,1,__ATOMIC_RELAXED);
 return ok;}
static const struct nested_ept_atomic_ops ops={guard,table,leaf,load,cas};
static void init(struct fixture *f){memset(pages,0,sizeof pages);*f=(struct fixture){.permissions=7};pages[0][0]=0x2007;pages[1][0]=0x3007;pages[2][2]=BASE|0xf7;}
static struct nested_ept_atomic_result walk(struct fixture *f,uint64_t g,unsigned need,struct nested_ept_atomic_trace *t){return nested_ept_atomic_walk_wb_ipat_2m(0x105e,g,need,48,&ops,f,stamp,t);}
static void offsets(void){struct fixture f;struct nested_ept_atomic_trace t;
 for(unsigned n=0;n<512;n++){init(&f);uint64_t g=L2+n*4096+4095;struct nested_ept_atomic_result r=walk(&f,g,1,&t);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&r.changed&&t.count==3);
 assert(r.translation.l1_gpa==BASE+n*4096+4095&&r.translation.hpa==physical(BASE+n*4096)+4095);
 assert(f.requested==BASE+n*4096&&f.leaf_calls==1&&!f.table_calls[3]);
 assert(t.entry[2].leaf&&t.entry[2].depth==2&&t.entry[2].leaf_shift==21&&t.entry[2].entry_gpa==0x3010);
 assert(!t.entry[0].leaf&&!t.entry[1].leaf&&t.entry[0].depth==0&&t.entry[1].depth==1);
 assert(r.translation.memory_explicit&&r.translation.ignore_pat&&r.translation.leaf_type==6&&r.translation.walk_type==6);
 assert((pages[2][2]&0x300)==0x100);}
 init(&f);struct nested_ept_atomic_result r=walk(&f,L2+0x200000,1,&t);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED&&t.count==2&&!f.leaf_calls);
 puts("PASS N44 independent 512 nonlinear physical subpage offsets and next-PDE boundary");}
static void formats(void){struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;
 const uint64_t bits[]={0x1000,0x100000,0x400,0x800,UINT64_C(1)<<48,UINT64_C(1)<<63};
 for(unsigned i=0;i<sizeof bits/sizeof *bits;i++){init(&f);pages[2][2]|=bits[i];r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed&&!f.leaf_calls);}
 for(unsigned i=0;i<8;i++)if(i!=6){init(&f);pages[2][2]=(pages[2][2]&~UINT64_C(0x38))|(i<<3);r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);}
 init(&f);pages[2][2]&=~UINT64_C(0x40);r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 for(unsigned i=0;i<2;i++){init(&f);pages[i][0]=BASE|0xf7;r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed&&!f.leaf_calls);}
 /* Policy exclusion must be tested with an otherwise aligned 1GiB base:
  * a misaligned base would hide a weakened PDPTE-PS policy check. */
 init(&f);pages[1][0]=UINT64_C(0x40000000)|0xf7;r=walk(&f,L2,1,&t);
 assert(r.status==NEA_UNSUPPORTED&&!r.changed&&!f.leaf_calls);
 init(&f);r=nested_ept_atomic_walk_wb_ipat(0x105e,L2,1,48,&ops,&f,stamp,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);pages[2][2]&=~UINT64_C(0x40);r=nested_ept_atomic_walk(0x105e,L2,1,48,&ops,&f,stamp,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);pages[2][2]=0x4007;pages[3][0]=BASE|0x77;r=walk(&f,L2,1,&t);assert(r.status==NEA_DONE&&t.count==4&&t.entry[3].depth==3&&t.entry[3].leaf_shift==12);
 puts("PASS N44 independent 2MiB reserved/format policy, legacy exclusion and mixed 4KiB path");}
static void permission_faults(void){struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;
 for(unsigned i=0;i<3;i++){init(&f);pages[i][i==2?2:0]&=~UINT64_C(2);r=walk(&f,L2+0x19037,3,&t);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED&&!(r.translation.effective12&2));assert((pages[2][2]&0x300)==0x100);
 init(&f);pages[i][i==2?2:0]&=~UINT64_C(2);f.deny_leaf=true;r=walk(&f,L2,3,&t);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);}
 for(unsigned i=0;i<3;i++){init(&f);pages[i][i==2?2:0]=UINT64_MAX&~UINT64_C(7);r=walk(&f,L2,3,&t);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED&&r.translation.effective12==0&&t.count==i&&!f.leaf_calls);assert(pages[i][i==2?2:0]==(UINT64_MAX&~UINT64_C(7)));}
 init(&f);f.permissions=1;r=walk(&f,L2,3,&t);assert(r.status==NEA_BACKING&&!r.changed&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED);
 init(&f);r=walk(&f,L2,3,&t);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&(pages[2][2]&0x300)==0x300);
 puts("PASS N44 independent effective permissions, AD1 R/W access, nonpresent ignored payload and L0 precedence");}
static void races(void){struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;init(&f);pages[2][2]|=0x200;f.clear_dirty=true;
 r=walk(&f,L2,1,&t);assert(r.status==NEA_DONE&&(pages[2][2]&0x300)==0x100);
 init(&f);f.structural=true;r=walk(&f,L2,2,&t);assert(r.status==NEA_REWALK&&!r.changed);
 init(&f);f.retry=true;r=walk(&f,L2,2,&t);assert(r.status==NEA_RETRY&&!r.changed&&f.compares==64);
 init(&f);f.wrong_hpa=true;r=walk(&f,L2,2,&t);assert(r.status==NEA_BACKING&&r.changed&&(pages[2][2]&0x300)==0);
 init(&f);r=walk(&f,L2,2,&t);assert(r.status==NEA_DONE);unsigned count=f.guards;bool partial=false;
 for(unsigned i=1;i<=count;i++){init(&f);f.deny_guard=i;r=walk(&f,L2,2,&t);assert(r.status==NEA_STALE);partial|=r.changed;}assert(partial);
 puts("PASS N44 independent CAS stale-D nonreplay, structural conflict, bounded retry, identity and every guard seam");}
struct thread {struct fixture f;unsigned need,subpage;};
static void *threadwalk(void *v){struct thread *t=v;struct nested_ept_atomic_trace trace;struct nested_ept_atomic_result r=walk(&t->f,L2+t->subpage*4096,t->need,&trace);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&trace.count==3);return NULL;}
static void concurrent(void){for(unsigned n=0;n<80;n++){struct fixture f;init(&f);pages[0][0]|=0x100;pages[1][0]|=0x100;unsigned collisions=0;pthread_barrier_t barrier;assert(!pthread_barrier_init(&barrier,0,2));
 struct thread t[2]={{.f={.permissions=7,.barrier=&barrier,.collisions=&collisions},.need=1,.subpage=1},{.f={.permissions=7,.barrier=&barrier,.collisions=&collisions},.need=2,.subpage=511}};pthread_t p[2];
 assert(!pthread_create(&p[0],0,threadwalk,&t[0])&&!pthread_create(&p[1],0,threadwalk,&t[1]));assert(!pthread_join(p[0],0)&&!pthread_join(p[1],0));assert(collisions>=1&&(pages[2][2]&0x300)==0x300);assert(!pthread_barrier_destroy(&barrier));}
 puts("PASS N44 independent 80 forced real CAS collisions on shared huge PDE across distinct subpages");}
struct cat {struct nested_root_catalog c;struct nrc_span spans[5];struct nrc_root roots[2];struct ad_updater_gate gate;struct nrc_identity id;unsigned calls;uint64_t deny;};
static bool authority(void *c,const struct nrc_span *s,uint64_t g,struct nrc_atomic_page *out){struct cat *f=c;f->calls++;if(g==f->deny)return false;uint64_t off=g-s->gpa;uint64_t *v=s->role==NRC_TABLE?(uint64_t *)((uintptr_t)s->table_view+off):0;
 *out=(struct nrc_atomic_page){.backing={.object=s->object,.lifetime=s->lifetime,.hpa=s->hpa+off,.table_view=v,.permissions=s->permissions,.memory_type=6,.ignore_pat=true},.atomic_view=v,.pinned_writable=v!=0};return true;}
static bool cg(void *c,struct nac_cache_identity id){return !memcmp(c,&id,sizeof id);}
static void ci(struct cat *f){memset(f,0,sizeof *f);struct fixture tmp;init(&tmp);pages[3][0]=0x2007;f->id=(struct nrc_identity){11,12,13,15};
 assert(nrc_init_atomic_wb_2m(&f->c,f->id,48,f->spans,5,f->roots,2,authority));
 struct nrc_span s={.identity=f->id,.object=1,.lifetime=22,.gpa=0x1000,.hpa=0x100000,.bytes=4*4096,.table_view=pages[0],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};assert(nrc_add_atomic_span(&f->c,s,pages[0]));
 for(unsigned n=0;n<2;n++){s=(struct nrc_span){.identity=f->id,.object=2+n,.lifetime=22,.gpa=BASE+(n?511:1)*4096,.hpa=0x1000000+n*0x400000,.bytes=4096,.permissions=7,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};assert(nrc_add_atomic_span(&f->c,s,0));}
 for(unsigned n=0;n<2;n++){struct nrc_root r={.identity=f->id,.source=n+1,.owner=1,.generation=7,.lifetime=25,.full_eptp=(n?0x4000:0x1000)|0x5e,.root_gpa=n?0x4000:0x1000};assert(nrc_add_root(&f->c,r));}
 assert(nrc_publish(&f->c)&&ad_updater_gate_init(&f->gate,stamp));}
static struct nac_ticket cb(struct cat *f,struct nac_permit *p,struct nac_cache_identity *cache,unsigned root){struct nac_ticket t;assert(nac_begin(&f->c,f->roots[root],f,&f->gate,stamp,*cache,cg,cache,p,&t));return t;}
static void traces(void){struct cat f;ci(&f);struct nac_cache_identity cache={0,0xa00000,0xa0005e,30,40};struct nac_permit p={0};struct nac_ticket k=cb(&f,&p,&cache,0);struct nac_trace t,b;struct nested_ept_atomic_result r=nac_walk(&p,k,L2+0x1037,1,&t);assert(r.status==NEA_DONE&&t.complete&&nac_trace_guard(&p,k,L2+0x1037,&t));assert(t.translation.hpa==0x1000037);
 for(unsigned n=0;n<3;n++){
 b=t;b.path.entry[n].depth++;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.path.entry[n].leaf_shift^=9;assert(!nac_trace_guard(&p,k,t.gpa,&b));
 b=t;b.path.entry[n].leaf=!b.path.entry[n].leaf;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.path.entry[n].pointer=(void *)1;assert(!nac_trace_guard(&p,k,t.gpa,&b));
 b=t;b.path.entry[n].entry_gpa+=8;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.path.entry[n].table_hpa+=4096;assert(!nac_trace_guard(&p,k,t.gpa,&b));}
 for(unsigned n=0;n<6;n++)if(n!=3){b=t;b.path.count=n;assert(!nac_trace_guard(&p,k,t.gpa,&b));}
 b=t;b.path.entry[2].raw^=0x80;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.translation.l1_gpa+=4096;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.translation.hpa+=4096;assert(!nac_trace_guard(&p,k,t.gpa,&b));
 b=t;b.gpa+=4096;assert(!nac_trace_guard(&p,k,b.gpa,&b));
 struct nac_trace other;r=nac_walk(&p,k,L2+0x1ff000,2,&other);assert(r.status==NEA_DONE&&other.translation.hpa==0x1400000&&nac_trace_guard(&p,k,t.gpa,&t));assert(t.path.entry[2].pointer==other.path.entry[2].pointer);
 r=nac_walk(&p,k,L2+0x2000,1,&other);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!other.complete);
 assert(nac_complete(&p,k));struct nac_ticket next=cb(&f,&p,&cache,0);assert(nac_trace_guard(&p,next,t.gpa,&t)&&!nac_guard(&p,k));assert(nac_complete(&p,next));
 next=cb(&f,&p,&cache,1);f.calls=0;assert(!nac_trace_guard(&p,next,t.gpa,&t)&&!f.calls);assert(nac_complete(&p,next));
 puts("PASS N44 independent trace depth/shift/count/source identities, peer huge D and sparse backing holes");}
struct pool {struct nested_sparse_page pages[16];unsigned count,frees,flushes;struct nac_permit *p;struct nac_ticket t;bool check;};
static bool alloc(void *c,struct nested_sparse_page *p){struct pool *x=c;assert(x->count<16);*p=(struct nested_sparse_page){.hpa=0x5000000+x->count*4096,.view=aligned_alloc(4096,4096)};if(!p->view)return false;x->pages[x->count++]=*p;return true;}
static void release(void *c,struct nested_sparse_page p){struct pool *x=c;x->frees++;free(p.view);}
static bool flush(void *c,uint64_t e){struct pool *x=c;x->flushes++;assert((e&4095)==0x5e);if(x->check)assert(nac_guard(x->p,x->t));return true;}
static uint64_t epte(struct pool *p,struct nested_ept_sparse *tree,uint64_t g){uint64_t h=tree->eptp&AM,r=0;for(unsigned d=0;d<4;d++){uint64_t *v=0;for(unsigned n=0;n<p->count;n++)if(p->pages[n].hpa==h)v=p->pages[n].view;assert(v);r=v[(g>>(39-d*9))&511];if(d<3)assert(!(r&0x80));h=r&AM;}return r;}
static void sparse(void){struct cat f;ci(&f);struct nested_ept_sparse tree;struct nac_permit p={0};struct pool pool={.p=&p};const struct nested_sparse_ops so={alloc,release,flush};assert(nested_ept_sparse_init_mode(&tree,&so,&pool,48,true));struct nac_cache_identity cache={0,tree.eptp&AM,tree.eptp,77,tree.generation};pool.t=cb(&f,&p,&cache,0);pool.check=true;
 __atomic_fetch_or(&pages[2][2],0x200,__ATOMIC_ACQ_REL);struct nac_trace t[2];uint64_t g[]={L2+0x1000,L2+0x1ff000};
 for(unsigned n=0;n<2;n++){struct nested_ept_atomic_result r=nac_walk(&p,pool.t,g[n],1,&t[n]);assert(r.status==NEA_DONE&&nested_ept_sparse_fill_tracked(&tree,g[n],&r.translation,true)==NESTED_SPARSE_OK);assert((epte(&pool,&tree,g[n])&7)==5);}
 assert(nac_complete(&p,pool.t));pool.t=cb(&f,&p,&cache,0);assert(nac_trace_guard(&p,pool.t,g[0],&t[0]));struct nac_trace w;assert(nac_walk(&p,pool.t,g[0],2,&w).status==NEA_DONE);assert(nested_ept_sparse_promote_write(&tree,g[0]));assert((epte(&pool,&tree,g[0])&7)==7&&(epte(&pool,&tree,g[1])&7)==5);assert(nac_complete(&p,pool.t));pool.check=false;assert(nested_ept_sparse_destroy(&tree)&&pool.frees==pool.count);
 puts("PASS N44 independent real sparse 4KiB trees track each subpage RO despite shared D and promote one only");}
int main(int argc,char **argv){const char *which=argc>1?argv[1]:"all";
#define RUN(n) if(!strcmp(which,"all")||!strcmp(which,#n))n()
 RUN(offsets);RUN(formats);RUN(permission_faults);RUN(races);RUN(concurrent);RUN(traces);RUN(sparse);return 0;}
