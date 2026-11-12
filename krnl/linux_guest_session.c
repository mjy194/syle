#include "linux_guest_session.h"
static void host_safe(struct lgs_session *s) {
 if(s->state.host_dirty || !s->platform.safe(s->platform.ctx)) {
  s->platform.fatal(s->platform.ctx);
  /* fatal must not return, including faulty test/backend implementations. */
  for(;;) {}
 }
}
bool lgs_configure(struct lgs_session*s,const struct lvh_ops*h,const struct lvs_ops*v,
 const struct lgs_platform*p,const void*k,size_t ks,const void*i,size_t is,const char*c,uint64_t rsp,uint64_t rip) {
 if(!s||!h||!v||!p||!p->enable||!p->disable||!p->safe||!p->read_field||!p->dispatch||!p->fatal||
 (!p->before_entry!=!p->after_entry)||!k||!ks||!i||!is||!c||s->configured||!rsp||(rsp&15)||!rip||s->prepared||s->enabled||s->host.active||
 s->host.vmcs_page||s->host.vmxon_page||s->memory.prepared)return false;
 s->hop=*h;s->sop=*v;s->platform=*p;s->kernel=k;s->kernel_size=ks;s->initrd=i;s->initrd_size=is;
 s->command_line=c;s->fallback_rsp=rsp;s->fallback_rip=rip;s->configured=true;return true;
}
static int prepare(void*ctx,uint64_t profile) {
 struct lgs_session*s=ctx;
 if(!s->configured||profile!=SYLE_GUEST_BOOT_PROFILE||s->prepared||s->enabled||s->failed)return 1;
 if(!lvs_init(&s->state))return 1;
 if(!lgm_prepare(&s->memory,&s->hop.memory,s->kernel,s->kernel_size,s->initrd,s->initrd_size,s->command_line))return 1;
 if(!s->platform.enable(s->platform.ctx))return 1;
 s->enabled=true;
 if(!lvh_open(&s->host,&s->hop,&s->memory,s->fallback_rsp,s->fallback_rip)||
    !s->hop.invept(s->hop.context,s->memory.eptp)) {host_safe(s);return 1;}
 if(s->host.caps.timer_shift>28) {host_safe(s);return 1;}
 for(unsigned j=0;j<15;j++)s->native.gpr[j]=s->host.plan.gpr[j];
 s->native.cr2=0;s->native.resume=0;s->exits=0;s->last_reason=0;s->prepared=true;
 host_safe(s);return 0;
}
static int start(void*ctx) {
 struct lgs_session*s=ctx;
 if(!s->prepared||s->failed||!s->enabled||!s->host.ready||!s->host.active)return 1;
 host_safe(s);
 /* Mandatory preemption timer in lvp_prepare bounds even a non-exiting guest.
  * One entry per call; retain VMCS launch state and all guest fields in place. */
 if(!s->hop.vmwrite(s->hop.context,0x482e,UINT64_C(1)<<(28-s->host.caps.timer_shift)))goto fail;
 if(s->platform.before_entry&&!s->platform.before_entry(s->platform.ctx,s))goto fail;
 enum lvs_result r=lvs_run(&s->state,&s->sop);
 host_safe(s);
 struct lgs_entry_result result={.state_result=r};
 uint64_t reason=0,vectoring=0;
 if(r==LVS_EXIT) {
  bool reason_ok=s->platform.read_field(s->platform.ctx,0x4402,&reason);
  bool vector_ok=s->platform.read_field(s->platform.ctx,0x4408,&vectoring);
  if(reason_ok&&vector_ok&&reason<=UINT32_MAX&&vectoring<=UINT32_MAX) {
   result.fields_valid=true;result.reason=(uint32_t)reason;result.vectoring=(uint32_t)vectoring;
  }
 }
 if(s->platform.after_entry&&!s->platform.after_entry(s->platform.ctx,s,&result))goto fail;
 if(r!=LVS_EXIT||!result.fields_valid||reason>UINT16_MAX||(vectoring&(UINT64_C(1)<<31)))goto fail;
 s->last_reason=reason;++s->exits;
 /* Preemption exits preserve guest RIP and activity. Other exits require an
  * explicit emulator; never reinterpret an unhandled exit as successful boot. */
 if(!s->platform.dispatch(s->platform.ctx,s,(uint32_t)reason))goto fail;
 s->native.resume=1;
 host_safe(s);return 0;
fail:
 s->failed=true;host_safe(s);return 1;
}
static int stop(void*ctx) {
 struct lgs_session*s=ctx;
 /* Entry is synchronous and timer bounded, so returning from start already
  * quiesced the CPU. Preserve VMCS rather than restarting the initial guest. */
 host_safe(s);return 0;
}
static int release(void*ctx) {
 struct lgs_session*s=ctx;
 host_safe(s);
 if(!lvh_close(&s->host))return 1;
 if(s->enabled) {
  if(!s->platform.disable(s->platform.ctx)) {s->platform.fatal(s->platform.ctx);for(;;){}}
  s->enabled=false;
 }
 host_safe(s);
 if(!lgm_release(&s->memory))return 1;
 s->prepared=false;s->failed=false;
 s->state=(struct lvs_state){0};
 /* native bind stays resident; fresh create resets mutable guest state only. */
 return 0;
}
const struct guest_backend lgs_guest_backend={.prepare=prepare,.start=start,.stop=stop,.release=release};
