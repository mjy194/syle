#include "guest.h"
#include "guest_resume_probe.h"
static int64_t call(void*p,uint64_t op,uint64_t h,uint64_t a){(void)p;return syle_guest_call(op,h,a);}
static bool output(void*p,uint8_t b){(void)p;return syle_guest_call(1,(uintptr_t)&b,1)==1;}
void mg_resume_main(void){
 const struct mg_continuous_ops ops={0,call,output};struct mg_resume_report report;
 enum mg_resume_result r=mg_resume_run(&ops,1u<<19,32,256,&report);
 const char*msg=r==MG_RESUME_VERIFIED?"\nSYLE.MGMT same-guest STOP START shell state verified\n":"\nSYLE.MGMT resume probe failed or budget exhausted\n";
 uint64_t n=0;while(msg[n])n++;bool ok=syle_guest_call(1,(uintptr_t)msg,n)==(int64_t)n;
 syle_guest_call(3,r!=MG_RESUME_VERIFIED||!ok,0);__builtin_trap();
}
