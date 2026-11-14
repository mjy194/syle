#include "../../user/mgmt/guest.h"
#ifndef NMI_PROBE_RUNNING
#define NMI_PROBE_RUNNING 0
#endif
static void say(const char *p,unsigned n){(void)syle_guest_call(1,(uint64_t)(uintptr_t)p,n);}
__attribute__((noreturn)) void nmi_probe_main(void){
 int64_t handle=syle_guest_call(SYLE_GUEST_CREATE,0,SYLE_GUEST_BOOT_PROFILE);
 if(handle<=0){static const char fail[]="NMI.PROBE.CREATE_FAIL\n";say(fail,sizeof fail-1);for(;;)__asm__ volatile("pause");}
#if NMI_PROBE_RUNNING
 if(syle_guest_call(SYLE_GUEST_START,(uint64_t)handle,0)){static const char fail[]="NMI.PROBE.START_FAIL\n";say(fail,sizeof fail-1);for(;;)__asm__ volatile("pause");}
 static const char ready[]="NMI.PROBE.RUNNING_WAIT\n";
#else
 static const char ready[]="NMI.PROBE.READY_WAIT\n";
#endif
 say(ready,sizeof ready-1);
 __asm__ volatile(".global nmi_probe_wait\nnmi_probe_wait:\npause\njmp nmi_probe_wait" ::: "memory");
 __builtin_unreachable();
}
