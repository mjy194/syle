#include "guest.h"
#include "guest_resume_probe.h"
#include "../../include/syle/host_console.h"
static int64_t call(void*p,uint64_t op,uint64_t h,uint64_t a){(void)p;return syle_guest_call(op,h,a);}
static bool output(void*p,uint8_t b){(void)p;return syle_guest_call(1,(uintptr_t)&b,1)==1;}
static bool say(const char*s){uint64_t n=0;while(s[n])++n;return syle_guest_call(1,(uintptr_t)s,n)==(int64_t)n;}
static bool handle(uint64_t h){
 char b[19]="0x0000000000000000";static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<16;i++)b[17-i]=hex[(h>>(4*i))&15];
 return say("\nSYLE.CLOCK_HANDLE ")&&say(b)&&say("\n");
}
static bool pause_host(void*p,uint64_t h){
 (void)p;if(syle_guest_call(SYLE_GUEST_STATUS,h,0)!=SYLE_GUEST_STOPPED||!handle(h)||!say("\nSYLE.CLOCK_PAUSED\n"))return false;
 /* Guest remains quiesced. No START/STEP/recreate until explicit host R. */
 for(uint64_t i=0;i<(UINT64_C(1)<<27);++i){
  int64_t b=syle_guest_call(SYLE_HOST_CONSOLE_RECV,0,0);
  if(b==SYLE_HOST_CONSOLE_EMPTY)continue;
  return b=='R'&&syle_guest_call(SYLE_GUEST_STATUS,h,0)==SYLE_GUEST_STOPPED;
 }
 return false;
}
void mg_clock_pause_main(void){
 const struct mg_continuous_ops ops={0,call,output};struct mg_resume_report report;
 enum mg_resume_result r=mg_resume_run_with_pause(&ops,1u<<19,32,256,&report,"/bin/baseline-probe clockpause\n","\n",pause_host);
 bool ok=say(r==MG_RESUME_VERIFIED?"\nSYLE.CLOCK_PAUSE_VERIFIED same handle and process; cleaned up\n":"\nSYLE.CLOCK_PAUSE_FAILED\n");
 syle_guest_call(3,r!=MG_RESUME_VERIFIED||!ok,0);__builtin_trap();
}
