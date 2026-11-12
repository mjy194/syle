#define _POSIX_C_SOURCE 200809L
#include "nested_atomic_catalog.h"
#include "nested_ept_atomic_format.h"
#include "nested_ept_sparse.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define UC_GPA UINT64_C(0x50000)
#define UC_HPA UINT64_C(0x90000)
#define WB_GPA UINT64_C(0x60000)
#define WB_HPA UINT64_C(0xa0000)
static _Alignas(4096) uint64_t tab[4][512];
struct fixture {struct nested_root_catalog c;struct nrc_span spans[6];struct nrc_root roots[1];struct ad_updater_gate gate;struct ad_entry_stamp stamp;};
struct owner {struct fixture *f;struct nac_permit p;struct nac_ticket ticket;struct nac_cache_identity cache;unsigned corrupt,drift,calls,guards;};
static bool authority(void *v,const struct nrc_span *s,uint64_t gpa,struct nrc_atomic_page *p){
 struct owner *o=v;o->calls++;
 uint64_t off=gpa-s->gpa;
 *p=(struct nrc_atomic_page){.backing={s->object,s->lifetime,s->hpa+off,s->table_view,s->permissions,s->gpa==UC_GPA?0:6,true},.atomic_view=(uint64_t *)s->table_view,.pinned_writable=s->role==NRC_TABLE};
 if(s->role==NRC_DATA)switch(o->corrupt){case 1:p->backing.memory_type^=6;break;case 2:p->backing.hpa+=4096;break;case 3:p->backing.lifetime++;break;case 4:p->backing.permissions|=4;break;case 5:p->backing.ignore_pat=false;break;}
 return true;
}
static bool cache_guard(void *v,struct nac_cache_identity c){struct owner *o=v;o->guards++;if(o->drift){o->drift=0;o->f->c.type_epoch++;}return c.owner==o->cache.owner&&c.hpa==o->cache.hpa&&c.full_eptp==o->cache.full_eptp&&c.generation==o->cache.generation&&c.lifetime==o->cache.lifetime;}
static void setup(struct fixture *f,bool uc){
 memset(f,0,sizeof *f);memset(tab,0,sizeof tab);struct nrc_identity id={100,2,300,4};
 if(uc)assert(nrc_init_atomic_uc(&f->c,id,48,f->spans,6,f->roots,1,authority,UC_GPA,UC_HPA,31));
 else assert(nrc_init_atomic_wb(&f->c,id,48,f->spans,6,f->roots,1,authority));
 for(unsigned i=0;i<4;i++){struct nrc_span s={.identity=id,.object=i+1,.lifetime=100+i,.gpa=(i+1)*4096,.hpa=(i+17)*4096,.bytes=4096,.table_view=tab[i],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};assert(nrc_add_atomic_span(&f->c,s,tab[i]));}
 for(unsigned i=0;i<2;i++){struct nrc_span s={.identity=id,.object=10+i,.lifetime=200+i,.gpa=i?WB_GPA:UC_GPA,.hpa=i?WB_HPA:UC_HPA,.bytes=4096,.permissions=3,.allowed_types=!i&&uc?NRC_TYPE_UC:NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};assert(nrc_add_atomic_span(&f->c,s,NULL));}
 tab[0][0]=0x2007;tab[1][0]=0x3007;tab[2][0]=0x4007;tab[3][5]=UC_GPA|(uc?0x43:0x73);tab[3][6]=WB_GPA|0x73;tab[3][7]=UC_GPA|(uc?0x43:0x73);
 assert(nrc_add_root(&f->c,(struct nrc_root){id,1,9,1,1,0x105e,0x1000}));assert(nrc_publish(&f->c));f->stamp=(struct ad_entry_stamp){100,2,300,41,4};assert(ad_updater_gate_init(&f->gate,f->stamp));
}
static void owner(struct owner *o,struct fixture *f,unsigned n){*o=(struct owner){.f=f,.cache={n,0xb0000+n*4096,(0xb0000+n*4096)|0x5e,50+n,1}};}
static bool begin(struct owner *o){return nac_begin(&o->f->c,o->f->roots[0],o,&o->f->gate,o->f->stamp,o->cache,cache_guard,o,&o->p,&o->ticket);}
static void stop(struct owner *o){assert(nac_end(&o->p,o->ticket));assert(!o->p.active);}
static void formats(void){
 const uint64_t mask=UINT64_C(0x0000fffffffff000);unsigned count=0;
 for(unsigned uc=0;uc<2;uc++)for(unsigned depth=0;depth<4;depth++)for(unsigned type=0;type<8;type++)for(unsigned ipat=0;ipat<2;ipat++)for(unsigned perm=1;perm<8;perm++)for(unsigned large=0;large<2;large++){
  uint64_t raw=UINT64_C(0x40000000)|(type<<3)|(ipat<<6)|perm|(large<<7);unsigned shift=999;
  bool expected=(perm&1)&&!large&&(depth==3?(type==6||(uc&&type==0&&!(perm&4)))&&ipat:type==0&&!ipat);
  bool ok=nea_decode_policy(raw,depth,mask,true,false,false,uc,&shift);assert(ok==expected);if(ok)assert(shift==(depth==3?12u:0u));count++;
 }
 assert(count==1792);unsigned shift;
 for(unsigned depth=0;depth<4;depth++)for(unsigned huge=0;huge<4;huge++)assert(!nea_decode_policy(UINT64_C(0x40000043),depth,mask,true,huge&1,huge&2,true,&shift)||(huge==0&&depth==3));
 puts("PASS independent formats: 1792 policy/type/depth/IPAT/permission/PS combinations");
}
static void constructor(void){
 for(unsigned bad=0;bad<12;bad++){struct fixture f={0};struct nested_root_catalog old=f.c;uint64_t gpa=UC_GPA,hpa=UC_HPA,epoch=31;unsigned width=48;
  switch(bad){case 0:gpa=0;break;case 1:hpa=0;break;case 2:gpa++;break;case 3:hpa++;break;case 4:gpa=UINT64_C(1)<<48;break;case 5:hpa=UINT64_C(1)<<48;break;case 6:epoch=0;break;case 7:width=11;break;case 8:width=49;break;case 9:width=12;break;case 10:gpa=UINT64_MAX-4095;break;case 11:hpa=UINT64_MAX-4095;break;}
  assert(!nrc_init_atomic_uc(&f.c,(struct nrc_identity){1,2,3,4},width,f.spans,6,f.roots,1,authority,gpa,hpa,epoch));assert(!memcmp(&old,&f.c,sizeof old));
 }
 for(unsigned bad=0;bad<10;bad++){struct fixture f={0};struct nrc_identity id={1,2,3,4};assert(nrc_init_atomic_uc(&f.c,id,48,f.spans,6,f.roots,1,authority,UC_GPA,UC_HPA,31));struct nrc_span s={.identity=id,.object=1,.lifetime=1,.gpa=UC_GPA,.hpa=UC_HPA,.bytes=4096,.permissions=3,.allowed_types=NRC_TYPE_UC,.role=NRC_DATA,.ignore_pat=true};
  switch(bad){case 0:s.gpa+=4096;break;case 1:s.hpa+=4096;break;case 2:s.gpa+=4096;s.hpa+=4096;break;case 3:s.bytes=8192;break;case 4:s.permissions|=4;break;case 5:s.allowed_types|=NRC_TYPE_WB;break;case 6:s.allowed_types=NRC_TYPE_WB;break;case 7:s.role=NRC_TABLE;s.permissions=1;s.table_view=tab[0];break;case 8:s.gpa-=4096;s.bytes=8192;s.allowed_types=NRC_TYPE_WB;break;case 9:s.hpa-=4096;s.bytes=8192;s.allowed_types=NRC_TYPE_WB;break;}
  assert(!nrc_add_atomic_span(&f.c,s,s.role==NRC_TABLE?tab[0]:NULL)&&!f.c.span_count);
 }
 struct fixture f;setup(&f,true);assert(f.c.type_epoch==31&&!f.c.atomic_1g&&!f.c.atomic_2m);
 puts("PASS independent constructor fail-atomicity and exact policy");
}
static void mappings(void){
 struct fixture f;setup(&f,true);struct owner o;owner(&o,&f,0);assert(begin(&o));struct nac_trace t;
 for(unsigned slot=5;slot<=7;slot++){struct nested_ept_atomic_result r=nac_walk(&o.p,o.ticket,slot*4096+123,1,&t);assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_OK);assert(r.translation.hpa==(slot==6?WB_HPA:UC_HPA)+123&&r.translation.l1_gpa==(slot==6?WB_GPA:UC_GPA)+123&&r.translation.leaf_type==(slot==6?6:0)&&r.translation.walk_type==6);assert(nac_trace_guard(&o.p,o.ticket,slot*4096+123,&t));assert(!(tab[3][slot]&0x200));}
 stop(&o);
 for(unsigned bad=1;bad<=5;bad++){setup(&f,true);owner(&o,&f,0);o.corrupt=bad;assert(begin(&o));struct nrc_atomic_page page={0},prior=page;assert(!nrc_resolve_atomic(&o.p.source,o.ticket.source_nonce,UC_GPA,1,&page)&&!memcmp(&page,&prior,sizeof page));struct nested_ept_atomic_result r=nac_walk(&o.p,o.ticket,0x5000,2,&t);assert(r.status==NEA_BACKING&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED&&!r.changed);assert(tab[0][0]==0x2007&&tab[3][5]==(UC_GPA|0x43));stop(&o);}
 puts("PASS independent mixed aliases/byte offsets/authority rejection before CAS");
}
static void denials(void){
 struct fixture f;struct owner o;struct nac_trace t;
 for(unsigned which=0;which<5;which++){setup(&f,true);owner(&o,&f,0);if(which==0)tab[3][5]&=~UINT64_C(2);if(which==1)tab[3][5]=UINT64_MAX&~UINT64_C(7);if(which==2)tab[2][0]=UINT64_MAX&~UINT64_C(7);if(which==3){tab[3][5]&=~UINT64_C(2);o.corrupt=1;}if(which==4)tab[3][5]|=0x30;assert(begin(&o));struct nested_ept_atomic_result r=nac_walk(&o.p,o.ticket,0x5000,2,&t);
  if(which<3){assert(r.status==NEA_DONE&&r.translation.status==NESTED_EPT_L1_DENIED);if(which==0)assert(tab[3][5]==(UC_GPA|0x141));else if(which==1)assert(tab[3][5]==(UINT64_MAX&~UINT64_C(7)));else assert(tab[2][0]==(UINT64_MAX&~UINT64_C(7)));assert(tab[0][0]==0x2107);}else{assert(r.status==NEA_BACKING&&!r.changed&&r.translation.status==NESTED_EPT_L0_BACKING_DENIED);assert(tab[0][0]==0x2007);}stop(&o);
 }
 puts("PASS independent denial precedence and ignored nonpresent payload");
}
static void traces(void){
 struct fixture f;setup(&f,true);struct owner o;owner(&o,&f,0);assert(begin(&o));struct nac_trace good;assert(nac_walk(&o.p,o.ticket,0x5000,1,&good).status==NEA_DONE);stop(&o);assert(begin(&o));
 for(unsigned bad=0;bad<7;bad++){struct nac_trace t=good;switch(bad){case 0:t.type_policy.epoch++;break;case 1:t.type_policy.uc_gpa+=4096;break;case 2:t.type_policy.uc_hpa+=4096;break;case 3:t.type_policy=(struct nac_type_policy){0};break;case 4:t.source.identity.lifetime++;break;case 5:t.cache.generation++;break;case 6:t.path.stamp.epoch++;break;}t.path.entry[0].pointer=(void *)1;unsigned n=o.calls;assert(!nac_trace_guard(&o.p,o.ticket,0x5000,&t)&&o.calls==n);}
 assert(nac_trace_guard(&o.p,o.ticket,0x5000,&good));stop(&o);
 owner(&o,&f,0);o.drift=1;assert(!begin(&o)&&!o.p.active&&!f.c.references&&!f.gate.references);
 f.c.type_epoch=31;owner(&o,&f,0);assert(begin(&o));o.drift=1;assert(!nac_guard(&o.p,o.ticket));stop(&o);
 f.c.type_epoch=31;owner(&o,&f,0);assert(begin(&o));struct nac_trace t=good;f.c.type_epoch=32;assert(!nac_trace_guard(&o.p,o.ticket,0x5000,&t));stop(&o);
 puts("PASS independent standalone fresh-permit type epoch and pre-dereference trace policy");
}
struct concurrent {pthread_barrier_t *barrier;bool rendezvoused;unsigned cas_calls,cas_fail,need;struct nested_ept_atomic_result r;};
static bool dguard(void *p,struct ad_entry_stamp s){(void)p;return s.epoch==41;}
static bool dtable(void *p,uint64_t g,uint64_t *h,uint64_t **v){(void)p;if(g<0x1000||g>0x4000||(g&4095))return false;*h=g+0x10000;*v=tab[(g>>12)-1];return true;}
static bool dleaf(void *p,uint64_t g,uint64_t *h,uint8_t *perm){(void)p;if(g!=UC_GPA)return false;*h=UC_HPA;*perm=3;return true;}
static bool dtype(void *p,uint64_t g,uint64_t *h,uint8_t *perm,uint8_t *t){if(!dleaf(p,g,h,perm))return false;*t=0;return true;}
static uint64_t dload(void *p,uint64_t *w){(void)p;return __atomic_load_n(w,__ATOMIC_ACQUIRE);}
static bool dcas(void *v,uint64_t *w,uint64_t expected,uint64_t desired){struct concurrent *c=v;c->cas_calls++;assert((expected&~UINT64_C(0x300))==(desired&~UINT64_C(0x300)));if(w==&tab[3][5]&&!c->rendezvoused){c->rendezvoused=true;int n=pthread_barrier_wait(c->barrier);assert(!n||n==PTHREAD_BARRIER_SERIAL_THREAD);}bool ok=__atomic_compare_exchange_n(w,&expected,desired,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);if(!ok)c->cas_fail++;return ok;}
static void *thread(void *v){struct concurrent *c=v;const struct nested_ept_atomic_ops ops={dguard,dtable,dleaf,dload,dcas};struct nested_ept_atomic_trace trace;c->r=nested_ept_atomic_walk_uc_ipat(0x105e,0x5000,c->need,48,&ops,dtype,c,(struct ad_entry_stamp){100,2,300,41,4},&trace);return NULL;}
static void races(void){unsigned failed=0;for(unsigned r=0;r<100;r++){memset(tab,0,sizeof tab);tab[0][0]=0x2107;tab[1][0]=0x3107;tab[2][0]=0x4107;tab[3][5]=UC_GPA|0x43;pthread_barrier_t b;assert(!pthread_barrier_init(&b,NULL,2));struct concurrent c[2]={{.barrier=&b,.need=1},{.barrier=&b,.need=2}};pthread_t threads[2];for(unsigned n=0;n<2;n++)assert(!pthread_create(&threads[n],NULL,thread,&c[n]));for(unsigned n=0;n<2;n++){assert(!pthread_join(threads[n],NULL));assert(c[n].r.status==NEA_DONE&&c[n].r.translation.leaf_type==0&&c[n].r.translation.status==NESTED_EPT_OK&&c[n].rendezvoused);failed+=c[n].cas_fail;}assert(tab[3][5]==(UC_GPA|0x343));assert(!pthread_barrier_destroy(&b));}assert(failed==100);puts("PASS independent 100 forced UC-terminal shared CAS conflicts with real two-thread host execution");}

static void legacy(void){
 for(unsigned api=0;api<4;api++){struct fixture f;setup(&f,true);struct concurrent c={0};const struct nested_ept_atomic_ops ops={dguard,dtable,dleaf,NULL,NULL};struct nested_ept_atomic_trace t;struct nested_ept_atomic_result r;
  switch(api){case 0:r=nested_ept_atomic_walk(0x105e,0x5000,1,48,&ops,&c,f.stamp,&t);break;case 1:r=nested_ept_atomic_walk_wb_ipat(0x105e,0x5000,1,48,&ops,&c,f.stamp,&t);break;case 2:r=nested_ept_atomic_walk_wb_ipat_2m(0x105e,0x5000,1,48,&ops,&c,f.stamp,&t);break;default:r=nested_ept_atomic_walk_wb_ipat_1g(0x105e,0x5000,1,48,&ops,&c,f.stamp,&t);break;}
  assert(r.status==NEA_UNSUPPORTED&&!r.changed&&tab[0][0]==0x2007);
 }
 puts("PASS independent old walker APIs remain UC closed");
}
struct sparse_ctx {unsigned allocated,freed,flushes,admitted;};
static bool salloc(void *v,struct nested_sparse_page *p){struct sparse_ctx *c=v;void *mem=aligned_alloc(4096,4096);assert(mem);p->view=mem;p->hpa=0x100000+(++c->allocated)*4096;return true;}
static void sfree(void *v,struct nested_sparse_page p){struct sparse_ctx *c=v;c->freed++;free(p.view);}
static bool sflush(void *v,uint64_t eptp){struct sparse_ctx *c=v;c->flushes++;return (eptp&4095)==0x5e||(eptp&4095)==0x1e;}
static bool sadmit(void *v,const struct nested_ept_result *r){struct sparse_ctx *c=v;c->admitted++;return r->hpa==UC_HPA;}
static void sparse(void){
 const struct nested_sparse_ops ops={salloc,sfree,sflush};
 for(unsigned ad=0;ad<2;ad++)for(unsigned mode=0;mode<4;mode++)for(unsigned type=0;type<8;type++){
  struct sparse_ctx c={0};struct nested_ept_sparse t;assert(nested_ept_sparse_init_mode(&t,&ops,&c,48,ad));
  bool ok=mode==0?nested_ept_sparse_set_typed_admission(&t,sadmit,&c):mode==1?nested_ept_sparse_set_typed_admission_pat(&t,sadmit,&c):mode==2?nested_ept_sparse_set_typed_admission_wt(&t,sadmit,&c):nested_ept_sparse_set_typed_admission_atomic_uc(&t,sadmit,&c);
  if(mode==3&&!ad){assert(!ok&&!t.typed_admit&&!t.typed_atomic_uc);assert(nested_ept_sparse_destroy(&t));continue;}assert(ok);
  assert(!nested_ept_sparse_set_typed_admission_atomic_uc(&t,sadmit,&c));
  struct nested_ept_result r={.status=NESTED_EPT_OK,.l1_gpa=UC_GPA,.hpa=UC_HPA,.effective12=3,.effective02=3,.leaf_resolved=true,.memory_explicit=true,.ignore_pat=true,.walk_type=6,.leaf_type=type};
  bool want=type==6||(!ad&&(type==0||(type==4&&mode==2)))||(ad&&mode==3&&type==0);
  unsigned n=c.allocated,flush=c.flushes;enum nested_sparse_fill result=nested_ept_sparse_fill_tracked(&t,0x5000,&r,ad&&want);
  assert((result==NESTED_SPARSE_OK)==want);if(!want)assert(c.allocated==n&&c.flushes==flush&&!t.leaves&&!c.admitted);
  assert(nested_ept_sparse_destroy(&t));assert(c.allocated==c.freed);
 }
 for(unsigned bad=0;bad<6;bad++){struct sparse_ctx c={0};struct nested_ept_sparse t;assert(nested_ept_sparse_init_mode(&t,&ops,&c,48,true));assert(nested_ept_sparse_set_typed_admission_atomic_uc(&t,sadmit,&c));
  struct nested_ept_result r={.status=NESTED_EPT_OK,.l1_gpa=UC_GPA,.hpa=UC_HPA,.effective12=3,.effective02=3,.leaf_resolved=true,.memory_explicit=true,.ignore_pat=true,.walk_type=6,.leaf_type=0};
  switch(bad){case 0:r.ignore_pat=false;break;case 1:r.effective12|=4;break;case 2:r.effective02|=4;break;case 3:r.walk_type=0;break;case 4:r.memory_explicit=false;break;case 5:r.hpa+=4096;break;}
  unsigned before=c.allocated;enum nested_sparse_fill got=nested_ept_sparse_fill_tracked(&t,0x5000,&r,true);assert(got!=NESTED_SPARSE_OK&&!t.leaves&&c.allocated==before&&c.flushes==1);assert(nested_ept_sparse_destroy(&t));
 }
 puts("PASS independent sparse opt-in cross-product and UC restriction enforcement");
}
int main(int argc,char **argv){setvbuf(stdout,NULL,_IONBF,0);const char *s=argc>1?argv[1]:"all";
#define RUN(name) if(!strcmp(s,"all")||!strcmp(s,#name))name()
 RUN(formats);RUN(constructor);RUN(mappings);RUN(denials);RUN(traces);RUN(races);RUN(legacy);RUN(sparse);return 0;}
