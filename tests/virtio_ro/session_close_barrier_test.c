#define main legacy_host_main
#include "../../krnl/tests/linux_vcpu_host.c"
#undef main
#include "../../krnl/linux_guest_role_binding.h"
#include "session_authority.h"
#include <setjmp.h>
static struct lgs_session session;
static struct lgr_binding binding;
static struct guest_control ctl;
static struct vsa authority;
static struct vtc_entry entries[2];
static struct vlm_handle pins[2];
static bool held[2],deny_barrier,partial,deny_end;
static unsigned barriers,disables,ram_releases,ends;
static uint64_t epoch,retain;
static bool disable_fail,expect_fatal;
static jmp_buf fatal_jump;
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,0x100000,0,2,8};
static bool begin(void *ctx){assert(ctx==&authority);assert(guest_control_call(&ctl,1,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBUSY);if(!vsa_begin(ctx))return false;epoch=authority.owned.epoch;return true;}
static bool end(void *ctx){ends++;assert(!session.enabled&&!m.live&&!session.memory.prepared);return !deny_end&&vsa_end(ctx);}
static uint64_t allocate_role(void *ctx,enum sg_memory_role role){assert(role==SG_MEMORY_VMXON||role==SG_MEMORY_VMCS);uint64_t pa=vsa_allocate(ctx,role);if(!pa)return 0;unsigned n=idx(pa);assert(pna_pin(epoch,entries[n].allocation,&pins[n]));held[n]=true;m.alloc++;m.used[n]=true;m.live++;return pa;}
static bool free_control(void *ctx,uint64_t pa){if(pa==retain)return false;if(!vsa_release(ctx,pa))return false;return release(0,pa);}
static void *map_control(void *ctx,uint64_t pa){enum vlm_kind k;if(ctx!=&authority||!vsa_query(ctx,pa,&k)||k!=VLM_VMX_CONTROL||!held[idx(pa)])return 0;return m.page[idx(pa)];}
static bool wb_control(void *ctx,uint64_t pa){return map_control(ctx,pa)!=0;}
static bool barrier(void *ctx){assert(ctx==&authority&&binding.close_inflight&&!session.host.active&&!session.host.ready&&!m.active&&session.enabled);
 assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)==-SYLE_GUEST_EBUSY);
 assert(guest_control_call(&ctl,1,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBUSY);
 barriers++;if(deny_barrier||epoch!=authority.owned.epoch)return false;
 for(unsigned n=0;n<2;n++)if(held[n]){if(!pna_unpin(epoch,pins[n]))return false;held[n]=false;if(partial){partial=false;return false;}}
 return true;
}
static bool enable(void *ctx){(void)ctx;return true;}
static bool disable(void *ctx){(void)ctx;assert(!session.host.active&&!m.active&&!m.live&&!held[0]&&!held[1]);disables++;return !disable_fail;}
static bool safe(void *ctx){(void)ctx;return true;}
static bool read_field(void *ctx,uint32_t field,uint64_t *v){(void)ctx;(void)field;(void)v;return false;}
static bool dispatch(void *ctx,struct lgs_session*s,uint32_t reason){(void)ctx;(void)s;(void)reason;return false;}
static void fatal(void *ctx){(void)ctx;if(expect_fatal)longjmp(fatal_jump,1);assert(!"unexpected dirty host");}
bool lvs_init(struct lvs_state *s){s->ready=true;return true;}
enum lvs_result lvs_run(struct lvs_state*s,const struct lvs_ops*o){(void)s;(void)o;assert(!"no guest execution");return LVS_ENTRY_FAILED;}
bool lgm_prepare(struct linux_guest_memory *p,const struct sg_memory_ops *o,const void*k,size_t kn,const void*i,size_t in,const char*cstr){(void)k;(void)kn;(void)i;(void)in;(void)cstr;*p=ram;p->owner=*o;return true;}
bool lgm_prepare_roles(struct linux_guest_memory*p,const struct sg_memory_ops*o,sg_allocate_role_fn role,const void*k,size_t kn,const void*i,size_t in,const char*cstr){assert(role==allocate_role);return lgm_prepare(p,o,k,kn,i,in,cstr);}
bool lgm_release(struct linux_guest_memory *p){assert(!m.active&&!m.live&&!session.enabled);ram_releases++;p->prepared=false;return true;}
int main(int argc,char **argv){
 (void)argv;assert(argc==1||argc==2);
 assert(!legacy_host_main());reset();
 struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof(bi);bi.boot_services_exited=1;bi.descriptor_size=sizeof(desc);bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof(desc);bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof(desc)};assert(pmm_init(&bi));
 authority.storage=entries;authority.capacity=2;ops.memory=(struct sg_memory_ops){&authority,0,free_control,map_control,wb_control};
 struct lvs_ops v={0};struct lgs_platform p={.enable=enable,.disable=disable,.safe=safe,.read_field=read_field,.dispatch=dispatch,.fatal=fatal};const char asset='x';
 assert(lgs_configure(&session,&ops,&v,&p,&asset,1,&asset,1,"",0x80000,0x400000));
 assert(lgr_configure_barrier(&binding,&session,&authority,allocate_role,begin,end,barrier,&authority));guest_control_init(&ctl,1,&lgr_guest_backend,&binding);
 assert(guest_control_call(&ctl,1,SYLE_GUEST_CREATE,0,1)==1&&held[0]&&held[1]);
 m.block=3;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)==-SYLE_GUEST_EQUARANTINED&&m.active&&!barriers&&!disables&&!ram_releases&&!ends);
 m.block=0;deny_barrier=true;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!m.active&&m.live==2&&barriers==1&&!disables);unsigned calls=m.calls;
 deny_barrier=false;uint64_t saved_epoch=epoch;epoch++;
 assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&held[0]&&held[1]&&m.calls==calls&&!disables);epoch=saved_epoch;
 partial=true;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!held[0]&&held[1]&&m.calls==calls&&!disables);
 retain=session.host.vmxon_page;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&m.live==1&&!held[1]&&!disables&&!ram_releases&&m.calls==calls);
 retain=0;
 if(argc==2){
  disable_fail=true;expect_fatal=true;
  if(!setjmp(fatal_jump)){(void)guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0);assert(!"disable failure returned");}
  assert(!m.live&&!m.active&&!held[0]&&!held[1]&&session.enabled&&!ram_releases&&!ends&&ctl.busy);
  puts("actual session disable failure: fatal, no RAM release/end/normal return PASS (host longjmp harness)");return 0;
 }
 deny_end=true;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!m.live&&disables==1&&ram_releases==1&&ends==1&&binding.base_released&&m.calls==calls);
 deny_end=false;assert(!guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)&&disables==1&&ram_releases==1&&ends==2&&!pmm_get_stats().allocated_pages&&m.calls==calls);
 puts("actual controller/session/lvh/PMM pins: VMXOFF/barrier/partial unpin/free/end REAP ordering PASS; LGM fixture, mocked VMX, no DMA");return 0;
}
