/* Reuse the independently tested ACTUAL PMM fixture, not its ordinal maps as
 * production authority. The original main remains compiled, not executed. */
#define main prior_fixture_main
#include "lgm_typed_host_test.c"
#undef main
#include "lgm_bounded_candidate.h"
static unsigned borrows,operations,fail_at;
static bool faulted;
static const unsigned char *source_image,*source_rd;
static size_t source_image_n,source_rd_n;
static char source_cmd[4096]="console=ttyS0 rdinit=/init";
static unsigned char scratch[4096];
static bool in_span(const void*p,size_t n,const void*base,size_t size){uintptr_t a=(uintptr_t)p,b=(uintptr_t)base;return a>=b&&a-b<=size&&n<=size-(a-b);}
static bool operation(void*v,uint64_t pa,enum sg_memory_role role,size_t off,const void*src,size_t n,bool zero){
 struct bridge*b=v;struct record*r=record(b,pa);struct vlm_handle lease;
 enum vlm_kind kind=role==SG_MEMORY_RAM?VLM_GUEST_RAM:role==SG_MEMORY_EPT?VLM_EPT_TABLE:0;
 if(faulted||borrows||!r||r->kind!=kind||!n||off>4096||n>4096-off)return false;
 if(!pna_pin(b->epoch,r->a,&lease))return false;
 borrows++;operations++;unsigned char*dst=b->bytes+(size_t)idx(pa)*4096+off;
 bool ok=operations!=fail_at;size_t count=ok?n:(n>3?3:1);
 if(zero)memset(dst,0,count);else memcpy(dst,src,count);
 if(!ok)faulted=true; /* partial current operation completed; never retry it */
 assert(pna_unpin(b->epoch,lease));borrows--;return ok;
}
bool cb_page_zero(void*v,uint64_t pa,enum sg_memory_role role,size_t off,size_t n){return operation(v,pa,role,off,0,n,true);}
bool cb_page_write(void*v,uint64_t pa,enum sg_memory_role role,size_t off,enum cs_source source,const void*p,size_t n){
 if(faulted||n>sizeof scratch)return false;
 switch(source){
 case CS_IMAGE:if(!in_span(p,n,source_image,source_image_n))return false;break;
 case CS_INITRD:if(!in_span(p,n,source_rd,source_rd_n))return false;break;
 case CS_CMD:if(!in_span(p,n,source_cmd,sizeof source_cmd))return false;break;
 case CS_PARAMS:if(n!=4096)return false;memcpy(scratch,p,n);p=scratch;break;
 case CS_ENTRY:if(n!=8&&n!=2)return false;memcpy(scratch,p,n);p=scratch;break;
 default:return false;
 }
 return operation(v,pa,role,off,p,n,false);
}
bool cb_page_u64(void*v,uint64_t pa,enum sg_memory_role role,size_t off,uint64_t value){
 unsigned char value_bytes[8];for(unsigned i=0;i<8;i++)value_bytes[i]=(unsigned char)(value>>(8*i));
 return operation(v,pa,role,off,value_bytes,8,false);
}
static void*forbidden_map(void*v,uint64_t pa){(void)v;(void)pa;assert(!"bounded candidate must not map");return 0;}
static bool bounded_release(void*v,uint64_t pa){assert(!borrows);return release(v,pa);}
int main(int argc,char**argv){
 assert(argc==3||argc==4);setup_pmm();size_t kn,rn;void*kernel=file(argv[1],&kn),*rd=file(argv[2],&rn);
 source_image=kernel;source_image_n=kn;source_rd=rd;source_rd_n=rn;
 struct bridge*b=calloc(1,sizeof *b);struct linux_guest_memory*m=calloc(1,sizeof *m);assert(b&&m);
 assert(!posix_memalign((void**)&b->bytes,4096,(size_t)TOTAL*4096));assert(pna_start(&b->epoch));
 struct sg_memory_ops legacy={b,0,release,mapping,wb};
 unsigned char*expected=0;
 if(argc==3){
  assert(lgm_prepare_roles(m,&legacy,alloc_lgm_role,kernel,kn,rd,rn,source_cmd));
  expected=malloc((size_t)COUNT*4096);assert(expected);memcpy(expected,b->bytes,(size_t)COUNT*4096);
  assert(lgm_release(m)&&pna_drain(b->epoch)&&pna_end(b->epoch));
  b->allocated=0;memset(b->records,0,sizeof b->records);assert(pna_start(&b->epoch));
 }else {fail_at=(unsigned)strtoul(argv[3],0,10);assert(fail_at);b->fail_release=BASE;}
 struct sg_memory_ops bounded={b,0,bounded_release,forbidden_map,wb};
 memset(b->bytes,0xa5,(size_t)TOTAL*4096);
 bool ok=cb_lgm_prepare_roles(m,&bounded,alloc_lgm_role,kernel,kn,rd,rn,source_cmd);
 if(argc==3){
  assert(ok&&seal(b,m)&&!faulted&&!borrows);
  assert(!memcmp(expected,b->bytes,(size_t)COUNT*4096));
  assert(cb_lgm_release(m));
  printf("actual LGM isolated bounded candidate + actual PMM parity: %u operations PASS\n",operations);
 }else{
  assert(!ok&&faulted&&operations==fail_at&&!borrows&&!m->prepared&&!m->eptp);
  assert(m->pages[0]==BASE&&pmm_get_stats().allocated_pages==1);
  assert(!cb_page_zero(b,BASE,SG_MEMORY_RAM,0,1)&&operations==fail_at);
  b->fail_release=0;assert(cb_lgm_release(m));
  printf("actual LGM partial operation %u + retained-release retry PASS\n",fail_at);
 }
 assert(!pmm_get_stats().allocated_pages&&pna_drain(b->epoch)&&pna_end(b->epoch));
 free(expected);free(b->bytes);free(m);free(b);free(kernel);free(rd);return 0;
}
