#include "linux_ept_probe.h"
#include "../efi/src/exceptions.h"
#ifndef MG_FIXED_INPUTS_VERIFIED
#error "managed EPT probe requires historical image hash gate"
#endif
static struct lep_context context;
static struct lgm_service_ops service;
static bool probe_dispatch(void*x,struct lgs_session*s,uint32_t reason){
 unsigned phase=context.phase;uint64_t rip=0,q=0,gpa=0;
 if(reason==48){
  if(!s->platform.read_field(s->platform.ctx,0x681e,&rip)||
   !s->platform.read_field(s->platform.ctx,0x6400,&q)||
   !s->platform.read_field(s->platform.ctx,0x2400,&gpa))return false;
 }
 bool ok=lep_dispatch(x,s,reason);
 if(reason==48){
  log_text("MANAGED.EPT phase=");log_hex(phase);log_text(" rip=");log_hex(rip);
  log_text(" qualification=");log_hex(q);log_text(" gpa=");log_hex(gpa);
  log_text(ok?" repaired; same RIP\n":" rejected\n");
 }
 if(ok&&context.complete)log_text("MANAGED.EPT all three permission retries and value checkpoints complete\n");
 return ok;
}
bool linux_guest_startup_install(void){
 if(!lep_init(&context,&service))return false;
 service.dispatch=probe_dispatch;return linux_guest_manager_install(&service);
}
