#define _GNU_SOURCE
#include "nested_atomic_catalog.h"
#include "nested_ept_sparse.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BASE UINT64_C(0xc0000000)
#define L2 UINT64_C(0x80000000)
#define SIZE UINT64_C(0x40000000)
#define AM UINT64_C(0x000ffffffffff000)
static _Alignas(4096) uint64_t pages[5][512];
static const struct ad_entry_stamp stamp={11,12,13,14,15};
struct fixture {unsigned guards,deny_guard,loads,compares,table_calls[5],leaf_calls;
 uint64_t requested,base;uint8_t permissions;bool deny_leaf,clear_dirty,structural,retry,wrong_hpa,low_hpa;
 pthread_barrier_t *barrier;unsigned *collisions;};
static uint64_t physical(uint64_t g){return UINT64_C(0x100000000)+(((g-BASE)>>12)^0x15555)*4096;}
static bool guard(void *c,struct ad_entry_stamp s){struct fixture *f=c;assert(!memcmp(&s,&stamp,sizeof s));return ++f->guards!=f->deny_guard;}
static bool table(void *c,uint64_t g,uint64_t *h,uint64_t **v){struct fixture *f=c;
 if(g<0x1000||g>0x5000||(g&4095))return false;
 unsigned n=g/4096-1;f->table_calls[n]++;*v=pages[n];*h=0x100000+n*4096;
 if(f->wrong_hpa&&n==1&&f->table_calls[n]>1)*h+=4096;
 return true;}
static bool leaf(void *c,uint64_t g,uint64_t *h,uint8_t *p){struct fixture *f=c;f->leaf_calls++;f->requested=g;
 if(g<f->base||g-f->base>=SIZE||(g&4095))return false;
 *h=f->low_hpa?0x200000:physical(g);*p=f->permissions;return !f->deny_leaf;}
static uint64_t load(void *c,uint64_t *p){struct fixture *f=c;f->loads++;
 if(f->clear_dirty&&f->loads==3)__atomic_fetch_and(&pages[1][2],~UINT64_C(0x200),__ATOMIC_ACQ_REL);
 return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static bool cas(void *c,uint64_t *p,uint64_t a,uint64_t b){struct fixture *f=c;f->compares++;
 if(f->retry)return false;
 if(f->structural){__atomic_fetch_xor(p,UINT64_C(0x80),__ATOMIC_ACQ_REL);return false;}
 if(f->barrier&&f->compares==1){assert(p==&pages[1][2]);int r=pthread_barrier_wait(f->barrier);assert(!r||r==PTHREAD_BARRIER_SERIAL_THREAD);}
 bool ok=__atomic_compare_exchange_n(p,&a,b,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
 if(!ok&&f->collisions)__atomic_fetch_add(f->collisions,1,__ATOMIC_RELAXED);
 return ok;}
static const struct nested_ept_atomic_ops ops={guard,table,leaf,load,cas};
static void init(struct fixture *f){memset(pages,0,sizeof pages);*f=(struct fixture){.permissions=7,.base=BASE};pages[0][0]=0x2007;pages[1][2]=BASE|0xf7;}
static struct nested_ept_atomic_result walk(struct fixture *f,uint64_t g,unsigned need,struct nested_ept_atomic_trace *t){return nested_ept_atomic_walk_wb_ipat_1g(0x105e,g,need,48,&ops,f,stamp,t);}
static void offsets(void){struct fixture f;struct nested_ept_atomic_trace t;init(&f);
 for(unsigned n=0;n<262144;n++){pages[1][2]=BASE|0xf7;uint64_t g=L2+(uint64_t)n*4096+4095;struct nested_ept_atomic_result r=walk(&f,g,1,&t);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&r.changed&&t.count==2);
 assert(r.translation.l1_gpa==BASE+(uint64_t)n*4096+4095&&r.translation.hpa==physical(BASE+(uint64_t)n*4096)+4095);
 assert(f.requested==BASE+(uint64_t)n*4096&&f.leaf_calls==n+1&&!f.table_calls[2]);
 assert(t.entry[1].leaf&&t.entry[1].depth==1&&t.entry[1].leaf_shift==30&&t.entry[1].entry_gpa==0x2010);
 assert(!t.entry[0].leaf&&t.entry[0].depth==0&&t.entry[0].leaf_shift==0);
 assert(r.translation.memory_explicit&&r.translation.ignore_pat&&r.translation.leaf_type==6&&r.translation.walk_type==6);
 assert((pages[1][2]&0x300)==0x100);}
 init(&f);struct nested_ept_atomic_result r=walk(&f,L2+SIZE,1,&t);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED&&t.count==1&&!f.leaf_calls);
 puts("PASS N45 independent all 262144 nonlinear-backed subpages at final byte and next-PDPTE boundary");}
static void formats(void){struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;
 for(unsigned bit=12;bit<30;bit++){init(&f);pages[1][2]|=UINT64_C(1)<<bit;r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed&&!f.leaf_calls);}
 const unsigned bits[]={10,11,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63};
 for(unsigned i=0;i<sizeof bits/sizeof *bits;i++){init(&f);pages[1][2]|=UINT64_C(1)<<bits[i];r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed&&!f.leaf_calls);}
 for(unsigned i=0;i<8;i++)if(i!=6){init(&f);pages[1][2]=(pages[1][2]&~UINT64_C(0x38))|(i<<3);r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);}
 for(unsigned p=0;p<8;p++)if(!(p&1)&&p){init(&f);pages[1][2]=(pages[1][2]&~UINT64_C(7))|p;r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);}
 init(&f);pages[1][2]&=~UINT64_C(0x40);r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);pages[0][0]=BASE|0xf7;r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed&&!f.leaf_calls);
 init(&f);r=nested_ept_atomic_walk_wb_ipat_2m(0x105e,L2,1,48,&ops,&f,stamp,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);r=nested_ept_atomic_walk_wb_ipat(0x105e,L2,1,48,&ops,&f,stamp,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);pages[1][2]&=~UINT64_C(0x40);r=nested_ept_atomic_walk(0x105e,L2,1,48,&ops,&f,stamp,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 init(&f);pages[1][2]=0x3007;pages[2][0]=BASE|0xf7;r=walk(&f,L2,1,&t);assert(r.status==NEA_DONE&&t.count==3&&t.entry[2].depth==2&&t.entry[2].leaf_shift==21);
 init(&f);pages[1][2]=0x3007;pages[2][0]=0x4007;pages[3][0]=BASE|0x77;r=walk(&f,L2,1,&t);assert(r.status==NEA_DONE&&t.count==4&&t.entry[3].depth==3&&t.entry[3].leaf_shift==12);
 pages[3][0]|=0x80;r=walk(&f,L2,1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);
 puts("PASS N45 independent every 1GiB alignment bit, reserved/format policy, old-API exclusion and mixed-depth paths");}
static void widths(void){struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;
 for(unsigned width=12;width<=48;width++){init(&f);f.low_hpa=true;f.base=0;pages[1][2]=0;pages[1][0]=0xf7;
  uint64_t g=(UINT64_C(1)<<(width<30?width:30))-1;
  r=nested_ept_atomic_walk_wb_ipat_1g(0x105e,g,1,width,&ops,&f,stamp,&t);
  if(width<30)assert(r.status!=NEA_DONE&&!r.changed);else assert(r.status==NEA_DONE&&r.translation.l1_gpa==g&&r.translation.hpa==0x200000+4095);
  if(width>=30){uint64_t base=((UINT64_C(1)<<width)-SIZE);pages[1][0]=base|0xf7;f.base=base;
   r=nested_ept_atomic_walk_wb_ipat_1g(0x105e,SIZE-1,1,width,&ops,&f,stamp,&t);assert(r.status==NEA_DONE&&r.translation.l1_gpa==(UINT64_C(1)<<width)-1);
   pages[1][0]|=UINT64_C(1)<<width;r=nested_ept_atomic_walk_wb_ipat_1g(0x105e,SIZE-1,1,width,&ops,&f,stamp,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed);}}
 puts("PASS N45 independent MAXPHYADDR widths12..48, width30 edge and top aligned base");}
static void permission_faults(void){struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;
 for(unsigned i=0;i<2;i++)for(unsigned need=2;need<=4;need+=2){init(&f);pages[i][i?2:0]&=~(uint64_t)need;r=walk(&f,L2+0x3ffff037,need,&t);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED&&!(r.translation.effective12&need));assert((pages[1][2]&0x300)==0x100);
 init(&f);pages[i][i?2:0]&=~(uint64_t)need;f.deny_leaf=true;r=walk(&f,L2,need,&t);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);}
 for(unsigned i=0;i<2;i++){init(&f);pages[i][i?2:0]=UINT64_MAX&~UINT64_C(7);r=walk(&f,L2,3,&t);
 assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED&&r.translation.effective12==0&&t.count==i&&!f.leaf_calls);assert(pages[i][i?2:0]==(UINT64_MAX&~UINT64_C(7)));}
 init(&f);f.permissions=1;r=walk(&f,L2,3,&t);assert(r.status==NEA_BACKING&&!r.changed&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED);
 init(&f);r=walk(&f,L2,3,&t);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&(pages[1][2]&0x300)==0x300);
 puts("PASS N45 independent ancestor/leaf W/X denial, nonpresent ignored payload and L0 precedence");}
static void races(void){struct fixture f;struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;init(&f);pages[1][2]|=0x200;f.clear_dirty=true;
 r=walk(&f,L2,1,&t);assert(r.status==NEA_DONE&&(pages[1][2]&0x300)==0x100);
 init(&f);f.structural=true;r=walk(&f,L2,2,&t);assert(r.status==NEA_REWALK&&!r.changed);
 init(&f);f.retry=true;r=walk(&f,L2,2,&t);assert(r.status==NEA_RETRY&&!r.changed&&f.compares==64);
 init(&f);f.wrong_hpa=true;r=walk(&f,L2,2,&t);assert(r.status==NEA_BACKING&&r.changed&&(pages[1][2]&0x300)==0);
 init(&f);r=walk(&f,L2,2,&t);assert(r.status==NEA_DONE);unsigned count=f.guards;bool partial=false;
 for(unsigned i=1;i<=count;i++){init(&f);f.deny_guard=i;r=walk(&f,L2,2,&t);assert(r.status==NEA_STALE);partial|=r.changed;}assert(partial);
 puts("PASS N45 independent CAS stale-D nonreplay, structure conflict, retry, identity and all guard seams");}
struct thread {struct fixture f;unsigned need,subpage;};
static void *threadwalk(void *v){struct thread *t=v;struct nested_ept_atomic_trace trace;struct nested_ept_atomic_result r=walk(&t->f,L2+(uint64_t)t->subpage*4096,t->need,&trace);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK&&trace.count==2);return NULL;}
static void concurrent(void){for(unsigned n=0;n<100;n++){struct fixture f;init(&f);pages[0][0]|=0x100;unsigned collisions=0;pthread_barrier_t barrier;assert(!pthread_barrier_init(&barrier,0,2));
 struct thread t[2]={{.f={.permissions=7,.base=BASE,.barrier=&barrier,.collisions=&collisions},.need=1,.subpage=511},{.f={.permissions=7,.base=BASE,.barrier=&barrier,.collisions=&collisions},.need=2,.subpage=262143}};pthread_t p[2];
 assert(!pthread_create(&p[0],0,threadwalk,&t[0])&&!pthread_create(&p[1],0,threadwalk,&t[1]));assert(!pthread_join(p[0],0)&&!pthread_join(p[1],0));assert(collisions>=1&&(pages[1][2]&0x300)==0x300);assert(!pthread_barrier_destroy(&barrier));}
 puts("PASS N45 independent 100 forced CAS collisions on shared PDPTE across far subpages");}
struct cat {struct nested_root_catalog c;struct nrc_span spans[6];struct nrc_root roots[2];struct ad_updater_gate gate;struct nrc_identity id;unsigned calls;uint64_t deny;};
static bool authority(void *c,const struct nrc_span *s,uint64_t g,struct nrc_atomic_page *out){struct cat *f=c;f->calls++;if(g==f->deny)return false;uint64_t off=g-s->gpa;uint64_t *v=s->role==NRC_TABLE?(uint64_t *)((uintptr_t)s->table_view+off):0;
 *out=(struct nrc_atomic_page){.backing={.object=s->object,.lifetime=s->lifetime,.hpa=s->hpa+off,.table_view=v,.permissions=s->permissions,.memory_type=6,.ignore_pat=true},.atomic_view=v,.pinned_writable=v!=0};return true;}
static bool cg(void *c,struct nac_cache_identity id){return !memcmp(c,&id,sizeof id);}
static const uint64_t offsets45[]={0x1ff000,0x200000,0x3ffff000};
static void ci_mode(struct cat *f,unsigned mode){memset(f,0,sizeof *f);struct fixture tmp;init(&tmp);pages[3][0]=0x2007;f->id=(struct nrc_identity){11,12,13,15};
 assert((mode==2?nrc_init_atomic_wb_1g:mode==1?nrc_init_atomic_wb_2m:nrc_init_atomic_wb)(&f->c,f->id,48,f->spans,6,f->roots,2,authority));
 struct nrc_span s={.identity=f->id,.object=1,.lifetime=22,.gpa=0x1000,.hpa=0x100000,.bytes=4*4096,.table_view=pages[0],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};assert(nrc_add_atomic_span(&f->c,s,pages[0]));
 for(unsigned n=0;n<3;n++){s=(struct nrc_span){.identity=f->id,.object=2+n,.lifetime=22,.gpa=BASE+offsets45[n],.hpa=0x1000000+n*0x400000,.bytes=4096,.permissions=7,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};assert(nrc_add_atomic_span(&f->c,s,0));}
 for(unsigned n=0;n<2;n++){struct nrc_root r={.identity=f->id,.source=n+1,.owner=1,.generation=7,.lifetime=25,.full_eptp=(n?0x4000:0x1000)|0x5e,.root_gpa=n?0x4000:0x1000};assert(nrc_add_root(&f->c,r));}
 assert(nrc_publish(&f->c)&&ad_updater_gate_init(&f->gate,stamp));}
static void ci(struct cat *f){ci_mode(f,2);}
static struct nac_ticket cb(struct cat *f,struct nac_permit *p,struct nac_cache_identity *cache,unsigned root){struct nac_ticket t;assert(nac_begin(&f->c,f->roots[root],f,&f->gate,stamp,*cache,cg,cache,p,&t));return t;}
static void constructors(void){
 struct nested_root_catalog c,before;struct nrc_span spans[1];struct nrc_root roots[1];struct nrc_identity id={1,2,3,4};
 for(unsigned width=0;width<=50;width++){
  memset(&c,0xa5,sizeof c);before=c;bool ok=nrc_init_atomic_wb_1g(&c,id,width,spans,1,roots,1,authority);
  if(width<30||width>48)assert(!ok&&!memcmp(&c,&before,sizeof c));
  else assert(ok&&c.atomic_wb&&c.atomic_2m&&c.atomic_1g&&c.maxphys==width);
 }
 for(unsigned bad=0;bad<6;bad++){
  memset(&c,0xa5,sizeof c);before=c;struct nrc_identity invalid=id;if(bad==0)invalid.domain=0;
  assert(!nrc_init_atomic_wb_1g(&c,invalid,48,bad==1?NULL:spans,bad==2?0:1,bad==3?NULL:roots,bad==4?0:1,bad==5?NULL:authority));
  assert(!memcmp(&c,&before,sizeof c));
 }
 for(unsigned mode=0;mode<2;mode++){
  struct cat f;ci_mode(&f,mode);assert(!f.c.atomic_1g&&f.c.atomic_2m==(mode==1));
  struct nac_cache_identity cache={0,0xa00000,0xa0005e,30,40};struct nac_permit p={0};struct nac_ticket k=cb(&f,&p,&cache,0);struct nac_trace t;
  struct nested_ept_atomic_result r=nac_walk(&p,k,L2+offsets45[0],1,&t);assert(r.status==NEA_UNSUPPORTED&&!r.changed&&!t.complete);assert(nac_complete(&p,k));
 }
 puts("PASS N45 independent constructor fail-atomic width/identity/storage checks and strict old catalog policies");
}
static void traces(void){struct cat f;ci(&f);struct nac_cache_identity cache={0,0xa00000,0xa0005e,30,40};struct nac_permit p={0};struct nac_ticket k=cb(&f,&p,&cache,0);struct nac_trace t,b;struct nested_ept_atomic_result r=nac_walk(&p,k,L2+offsets45[0]+55,1,&t);assert(r.status==NEA_DONE&&t.complete&&nac_trace_guard(&p,k,t.gpa,&t));assert(t.translation.hpa==0x1000037);
 for(unsigned n=0;n<2;n++){
 b=t;b.path.entry[n].depth++;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.path.entry[n].leaf_shift^=9;assert(!nac_trace_guard(&p,k,t.gpa,&b));
 b=t;b.path.entry[n].leaf=!b.path.entry[n].leaf;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.path.entry[n].pointer=(void *)1;assert(!nac_trace_guard(&p,k,t.gpa,&b));
 b=t;b.path.entry[n].entry_gpa+=8;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.path.entry[n].table_hpa+=4096;assert(!nac_trace_guard(&p,k,t.gpa,&b));}
 for(unsigned n=0;n<7;n++)if(n!=2){b=t;b.path.count=n;assert(!nac_trace_guard(&p,k,t.gpa,&b));}
 b=t;b.path.entry[1].raw^=0x80;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.translation.l1_gpa+=4096;assert(!nac_trace_guard(&p,k,t.gpa,&b));b=t;b.translation.hpa+=4096;assert(!nac_trace_guard(&p,k,t.gpa,&b));
 b=t;b.gpa+=4096;assert(!nac_trace_guard(&p,k,b.gpa,&b));
 #define ID_BAD(field) b=t;b.field++;f.calls=0;assert(!nac_trace_guard(&p,k,t.gpa,&b)&&!f.calls)
 ID_BAD(source.identity.domain);ID_BAD(source.identity.incarnation);ID_BAD(source.identity.catalog);ID_BAD(source.identity.lifetime);
 ID_BAD(source.source);ID_BAD(source.owner);ID_BAD(source.generation);ID_BAD(source.lifetime);ID_BAD(source.full_eptp);ID_BAD(source.root_gpa);
 ID_BAD(cache.owner);ID_BAD(cache.hpa);ID_BAD(cache.full_eptp);ID_BAD(cache.lifetime);ID_BAD(cache.generation);
 ID_BAD(path.stamp.domain_id);ID_BAD(path.stamp.incarnation);ID_BAD(path.stamp.context_id);ID_BAD(path.stamp.epoch);ID_BAD(path.stamp.lifetime);
 #undef ID_BAD
 struct nac_trace other;for(unsigned n=1;n<3;n++){r=nac_walk(&p,k,L2+offsets45[n],2,&other);assert(r.status==NEA_DONE&&other.translation.hpa==0x1000000+n*0x400000&&nac_trace_guard(&p,k,t.gpa,&t));assert(t.path.entry[1].pointer==other.path.entry[1].pointer);}
 r=nac_walk(&p,k,L2+0x2000,1,&other);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!other.complete);
 assert(nac_complete(&p,k));struct nac_ticket next=cb(&f,&p,&cache,0);assert(nac_trace_guard(&p,next,t.gpa,&t)&&!nac_guard(&p,k));assert(nac_complete(&p,next));
 next=cb(&f,&p,&cache,1);f.calls=0;assert(!nac_trace_guard(&p,next,t.gpa,&t)&&!f.calls);assert(nac_complete(&p,next));
 puts("PASS N45 independent durable 2-entry traces, all identity/depth/count corruptions, far-page D and sparse holes");}
struct pool {struct nested_sparse_page pages[16];unsigned count,frees,flushes;struct nac_permit *p;struct nac_ticket t;bool check;};
static bool alloc(void *c,struct nested_sparse_page *p){struct pool *x=c;assert(x->count<16);*p=(struct nested_sparse_page){.hpa=0x5000000+x->count*4096,.view=aligned_alloc(4096,4096)};if(!p->view)return false;x->pages[x->count++]=*p;return true;}
static void release(void *c,struct nested_sparse_page p){struct pool *x=c;x->frees++;free(p.view);}
static bool flush(void *c,uint64_t e){struct pool *x=c;x->flushes++;assert((e&4095)==0x5e);if(x->check)assert(nac_guard(x->p,x->t));return true;}
static uint64_t epte(struct pool *p,struct nested_ept_sparse *tree,uint64_t g){uint64_t h=tree->eptp&AM,r=0;for(unsigned d=0;d<4;d++){uint64_t *v=0;for(unsigned n=0;n<p->count;n++)if(p->pages[n].hpa==h)v=p->pages[n].view;assert(v);r=v[(g>>(39-d*9))&511];if(d<3)assert(!(r&0x80));h=r&AM;}return r;}
static void sparse(void){struct cat f;ci(&f);struct nested_ept_sparse tree;struct nac_permit p={0};struct pool pool={.p=&p};const struct nested_sparse_ops so={alloc,release,flush};assert(nested_ept_sparse_init_mode(&tree,&so,&pool,48,true));struct nac_cache_identity cache={0,tree.eptp&AM,tree.eptp,77,tree.generation};pool.t=cb(&f,&p,&cache,0);pool.check=true;
 __atomic_fetch_or(&pages[1][2],0x200,__ATOMIC_ACQ_REL);struct nac_trace t[3];
 for(unsigned n=0;n<3;n++){uint64_t g=L2+offsets45[n];struct nested_ept_atomic_result r=nac_walk(&p,pool.t,g,1,&t[n]);assert(r.status==NEA_DONE&&nested_ept_sparse_fill_tracked(&tree,g,&r.translation,true)==NESTED_SPARSE_OK);assert((epte(&pool,&tree,g)&7)==5);}
 assert(nac_complete(&p,pool.t));pool.t=cb(&f,&p,&cache,0);assert(nac_trace_guard(&p,pool.t,t[0].gpa,&t[0]));struct nac_trace w;assert(nac_walk(&p,pool.t,t[0].gpa,2,&w).status==NEA_DONE);assert(nested_ept_sparse_promote_write(&tree,t[0].gpa));assert((epte(&pool,&tree,t[0].gpa)&7)==7);for(unsigned n=1;n<3;n++)assert((epte(&pool,&tree,t[n].gpa)&7)==5);assert(nac_complete(&p,pool.t));pool.check=false;assert(nested_ept_sparse_destroy(&tree)&&pool.frees==pool.count);
 puts("PASS N45 independent private 4KiB trees: 2MiB-crossing/GiB-end subpages stay RO despite peer huge D");}
int main(int argc,char **argv){const char *which=argc>1?argv[1]:"all";
#define RUN(n) if(!strcmp(which,"all")||!strcmp(which,#n))n()
 RUN(offsets);RUN(formats);RUN(widths);RUN(permission_faults);RUN(races);RUN(concurrent);RUN(constructors);RUN(traces);RUN(sparse);return 0;}
