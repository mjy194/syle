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

#include "managed_bounded_candidate.h"
static struct lgs_session session;
static struct guest_control controller;
static uint64_t managed_handle;
static struct bridge *owner;
static uint64_t control_on,control_cs;
static bool close_fail,end_fail;
static unsigned closes,ends;
static bool enable_host(void*x){(void)x;return true;}
static bool disable_host(void*x){(void)x;assert(!session.host.active);return true;}
static bool safe_host(void*x){(void)x;return true;}
static bool field(void*x,uint32_t f,uint64_t*v){(void)x;(void)f;*v=52;return true;}
static bool dispatch(void*x,struct lgs_session*s,uint32_t r){(void)x;(void)s;(void)r;return true;}
static void fatal(void*x){(void)x;assert(!"unexpected host fatal");}
static bool vmwrite(void*x,uint32_t f,uint64_t v){(void)x;(void)f;(void)v;return true;}
static bool invept(void*x,uint64_t p){(void)x;(void)p;return true;}
bool lvs_init(struct lvs_state*s){s->ready=s->guest_valid=true;return true;}
enum lvs_result lvs_run(struct lvs_state*s,const struct lvs_ops*o){(void)s;(void)o;assert(!"guest entry not part of host fixture");return LVS_ENTRY_FAILED;}
bool lvh_open(struct lvh_context*h,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip){(void)h;(void)o;(void)m;(void)rsp;(void)rip;assert(!"legacy host path");return false;}
bool mbp_host_open(struct lvh_context*h,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip,sg_allocate_role_fn role){
 (void)m;(void)rsp;(void)rip;assert(source_owner.held&&session.enabled);
 control_on=role(o->memory.context,SG_MEMORY_VMXON);control_cs=role(o->memory.context,SG_MEMORY_VMCS);assert(control_on&&control_cs);
 h->active=h->ready=true;h->caps.timer_shift=4;return true;
}
bool lvh_close(struct lvh_context*h){closes++;if(close_fail)return false;
 h->active=h->ready=false;if(control_cs){assert(release(owner,control_cs));control_cs=0;}if(control_on){assert(release(owner,control_on));control_on=0;}return true;
}
bool lvh_close_with_barrier(struct lvh_context*h,bool(*barrier)(void*),void*c){(void)h;(void)barrier;(void)c;assert(!"not installed");return false;}
static uint64_t managed_role(void*v,enum sg_memory_role r){
 if(r==SG_MEMORY_VMXON||r==SG_MEMORY_VMCS)return alloc_kind(v,VLM_VMX_CONTROL);
 return bounded_allocate(v,r);
}
static bool begin(void*v){
 struct bridge*b=v;
 if(session.kernel!=source_image||session.kernel_size!=source_image_n||session.initrd!=source_rd||session.initrd_size!=source_rd_n||session.command_line!=source_cmd)return false;
 assert(guest_control_call(&controller,7,SYLE_GUEST_CREATE,0,1)==-(int64_t)SYLE_GUEST_EBUSY);
 assert(guest_control_call(&controller,7,SYLE_GUEST_REAP,0,0)==-(int64_t)SYLE_GUEST_EBUSY);
 assert(pna_start(&b->epoch));assert(src_init(&source_owner,artifacts)&&src_acquire(&source_owner,&source_ref));
 struct css_ops ops={b,admit_source,transfer_owned};return css_init(&scope,b->epoch,&ops,&operation_handle);
}
bool mbp_prepare_done(void*v){assert(v==owner);return css_close(&scope,operation_handle);}
static bool end(void*v){
 struct bridge*b=v;ends++;if(!b->epoch){assert(!source_owner.initialized&&!pmm_get_stats().allocated_pages);return true;}assert(!session.enabled&&!session.host.active&&!session.memory.prepared&&source_owner.held&&scope.closing);
 if(end_fail)return false;
 assert(!pmm_get_stats().allocated_pages&&pna_drain(b->epoch)&&pna_end(b->epoch));return src_release(source_ref)&&src_retire(&source_owner);
}
static int controller_prepare(void){
 int64_t r=guest_control_call(&controller,7,SYLE_GUEST_CREATE,0,1);
 if(r>0){managed_handle=(uint64_t)r;return 0;}return 1;
}
static int controller_release(void){
 uint64_t op=controller.state==SYLE_GUEST_QUARANTINED?SYLE_GUEST_REAP:SYLE_GUEST_DESTROY;
 return guest_control_call(&controller,7,op,op==SYLE_GUEST_REAP?0:managed_handle,0)!=0;
}
int main(int argc,char**argv){
 assert(argc==3||argc==4);setup_pmm();size_t kn,rn;void*kernel=file(argv[1],&kn),*rd=file(argv[2],&rn);
 source_image=kernel;source_image_n=kn;source_rd=rd;source_rd_n=rn;
 artifacts[0]=(struct src_span){kernel,kn};artifacts[1]=(struct src_span){rd,rn};artifacts[2]=(struct src_span){source_cmd,sizeof source_cmd};
 owner=calloc(1,sizeof *owner);assert(owner&&!posix_memalign((void**)&owner->bytes,4096,(size_t)TOTAL*4096));
 struct lvh_ops h={.memory={owner,0,bounded_release,forbidden_map,wb},.vmwrite=vmwrite,.invept=invept};
 struct lvs_ops v={0};struct lgs_platform p={.enable=enable_host,.disable=disable_host,.safe=safe_host,.read_field=field,.dispatch=dispatch,.fatal=fatal};
 struct lgr_binding binding={0};assert(lgs_configure(&session,&h,&v,&p,kernel,kn,rd,rn,source_cmd,4096,4096));assert(mbp_configure(&binding,&session,owner,managed_role,begin,end));
 guest_control_init(&controller,7,&mbp_guest_backend,&binding);
 if(argc==4&&!strncmp(argv[3],"identity",8)){
  if(!strcmp(argv[3],"identity-kernel-size"))session.kernel_size++;
  else if(!strcmp(argv[3],"identity-initrd"))session.initrd=(const void*)(uintptr_t)1;
  else if(!strcmp(argv[3],"identity-initrd-size"))session.initrd_size++;
  else if(!strcmp(argv[3],"identity-cmd"))session.command_line=(const char*)(uintptr_t)1;
  else session.kernel=(const void*)(uintptr_t)1; /* reject BEFORE inspect */
  assert(controller_prepare()&&!binding.active&&!pmm_get_stats().allocated_pages&&!operations&&!source_owner.initialized&&ends==1);
  puts("actual CREATE rejects mismatched source identity before inspect; cleanup PASS");
  free(owner->bytes);free(owner);free(kernel);free(rd);return 0;
 }
 if(argc==4){fail_at=(unsigned)strtoul(argv[3],0,10);owner->fail_release=BASE;assert(controller_prepare());assert(scope.closing&&source_owner.held&&!session.enabled&&!session.host.active&&pmm_get_stats().allocated_pages==1);assert(controller_prepare());owner->fail_release=0;}
 else {
  assert(!controller_prepare()&&binding.ready&&session.prepared&&scope.closing&&source_owner.held&&pmm_get_stats().allocated_pages==TOTAL);
  close_fail=true;assert(controller_release()&&source_owner.held&&pmm_get_stats().allocated_pages==TOTAL);close_fail=false;
  owner->fail_release=BASE;assert(controller_release()&&pmm_get_stats().allocated_pages==1&&source_owner.held&&!ends);owner->fail_release=0;
 }
 end_fail=true;assert(controller_release()&&binding.base_released&&source_owner.held&&!pmm_get_stats().allocated_pages&&ends==1);
 unsigned saved=closes;end_fail=false;assert(!controller_release()&&!binding.active&&closes==saved&&ends==2&&!source_owner.held&&source_owner.retired);
 printf("actual CREATE/REAP + session/LGM/PMM source scope, mocked VMX %u operations PASS\n",operations);
 free(owner->bytes);free(owner);free(kernel);free(rd);return 0;
}
