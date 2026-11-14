/* Reuse the independently tested ACTUAL PMM fixture, not its ordinal maps as
 * production authority. The original main remains compiled, not executed. */
#define main prior_fixture_main
#include "lgm_typed_host_test.c"
#undef main
#include "lgm_bounded_candidate.h"
#include "session_scope.h"
#include "source_session.h"
static unsigned operations,fail_at,source_checks;
static struct css scope;
static struct css_handle operation_handle;
static struct src_owner source_owner;
static struct src_ref source_ref;
static struct src_span artifacts[3];
static const unsigned char *source_image,*source_rd;
static size_t source_image_n,source_rd_n;
static char source_cmd[4096]="console=ttyS0 rdinit=/init";
static unsigned char scratch[4096];
static enum cs_source pending_source;
static bool pending_zero,pending_value;
static void assert_source_retained(void){
 assert(source_owner.held&&!src_retire(&source_owner)&&!src_replace(&source_owner,artifacts));source_checks++;
}
static bool admit_source(void*ctx,const void*p,size_t n){
 (void)ctx;assert(scope.busy&&scope.borrows==1&&scope.pinned);assert_source_retained();
 if(n>4096)return false;
 if(pending_zero){memset(scratch,0,n);return true;}
 if(pending_value){if(n!=8)return false;memcpy(scratch,p,n);return true;}
 switch(pending_source){
 case CS_IMAGE:if(!src_contains(source_ref,0,p,n))return false;break;
 case CS_INITRD:if(!src_contains(source_ref,1,p,n))return false;break;
 case CS_CMD:if(!src_contains(source_ref,2,p,n))return false;break;
 case CS_PARAMS:if(n!=4096)return false;break;
 case CS_ENTRY:if(n!=8&&n!=2)return false;break;
 default:return false;
 }
 memcpy(scratch,p,n);return true;
}
static bool transfer_owned(void*ctx,uint64_t pa,size_t off,const void*p,size_t n){
 (void)p;struct bridge*b=ctx;assert(scope.busy&&scope.borrows==1&&scope.pinned&&source_owner.held);
 operations++;bool ok=operations!=fail_at;
 memcpy(b->bytes+(size_t)idx(pa)*4096+off,scratch,ok?n:(n>3?3:1));return ok;
}
static bool operation(void*v,uint64_t pa,enum sg_memory_role role,size_t off,const void*p,size_t n){
 struct bridge*b=v;struct record*r=record(b,pa);
 enum vlm_kind k=role==SG_MEMORY_RAM?VLM_GUEST_RAM:role==SG_MEMORY_EPT?VLM_EPT_TABLE:0;
 if(!r||r->kind!=k)return false;
 return css_write(&scope,operation_handle,r->a,k,off,p,n);
}
bool cb_page_zero(void*v,uint64_t pa,enum sg_memory_role role,size_t off,size_t n){
 pending_zero=true;pending_value=false;return operation(v,pa,role,off,scratch,n);
}
bool cb_page_write(void*v,uint64_t pa,enum sg_memory_role role,size_t off,enum cs_source source,const void*p,size_t n){
 pending_zero=false;pending_value=false;pending_source=source;return operation(v,pa,role,off,p,n);
}
bool cb_page_u64(void*v,uint64_t pa,enum sg_memory_role role,size_t off,uint64_t value){
 unsigned char value_bytes[8];for(unsigned i=0;i<8;i++)value_bytes[i]=(unsigned char)(value>>(8*i));
 pending_zero=false;pending_value=true;return operation(v,pa,role,off,value_bytes,8);
}
static uint64_t bounded_allocate(void*v,enum sg_memory_role role){assert_source_retained();return alloc_lgm_role(v,role);}
static void*forbidden_map(void*v,uint64_t pa){(void)v;(void)pa;assert(!"bounded candidate must not map");return 0;}
static bool bounded_release(void*v,uint64_t pa){assert(!scope.borrows);return release(v,pa);}
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
 artifacts[0]=(struct src_span){source_image,source_image_n};artifacts[1]=(struct src_span){source_rd,source_rd_n};artifacts[2]=(struct src_span){source_cmd,sizeof source_cmd};
 assert(src_init(&source_owner,artifacts)&&src_acquire(&source_owner,&source_ref));
 struct css_ops scoped_ops={b,admit_source,transfer_owned};assert(css_init(&scope,b->epoch,&scoped_ops,&operation_handle));
 assert_source_retained(); /* before candidate's FIRST inspect */
 struct sg_memory_ops bounded={b,0,bounded_release,forbidden_map,wb};
 memset(b->bytes,0xa5,(size_t)TOTAL*4096);
 bool ok=cb_lgm_prepare_roles(m,&bounded,bounded_allocate,kernel,kn,rd,rn,source_cmd);
 assert(css_close(&scope,operation_handle)); /* revoke before explicit owner release/retry */
 if(argc==3){
  assert(ok&&seal(b,m)&&!scope.faulted&&!scope.borrows);
  assert(!memcmp(expected,b->bytes,(size_t)COUNT*4096));
  assert(cb_lgm_release(m));
  printf("actual LGM source-session + scoped PMM parity: %u operations PASS\n",operations);
 }else{
  assert(!ok&&scope.faulted&&operations==fail_at&&!scope.borrows&&!m->prepared&&!m->eptp);
  assert(m->pages[0]==BASE&&pmm_get_stats().allocated_pages==1);
  assert(!cb_page_zero(b,BASE,SG_MEMORY_RAM,0,1)&&operations==fail_at);
  b->fail_release=0;assert(cb_lgm_release(m));
  printf("source-held actual LGM partial operation %u + retained-release retry PASS\n",fail_at);
 }
 assert(!pmm_get_stats().allocated_pages&&pna_drain(b->epoch)&&pna_end(b->epoch));
 assert(source_checks&&source_owner.held&&css_close(&scope,operation_handle));
 struct src_ref oldref=source_ref;assert(src_release(source_ref));
 assert(src_replace(&source_owner,artifacts)&&src_acquire(&source_owner,&source_ref));
 assert(!src_contains(oldref,0,kernel,1)&&!src_release(oldref)&&source_owner.held);
 assert(src_contains(source_ref,0,kernel,kn)&&src_release(source_ref)&&src_retire(&source_owner));
 assert(!src_acquire(&source_owner,&source_ref));
 free(expected);free(b->bytes);free(m);free(b);free(kernel);free(rd);return 0;
}
