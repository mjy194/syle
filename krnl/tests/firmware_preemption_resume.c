#include "../firmware_preemption_resume.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(fpr_resume(52,0x38,0x10006,0,0,0x400,0,0));
 assert(fpr_resume(52,0x38,6,0,0,0x400,0,0));
 assert(!fpr_resume(48,0x38,0x10006,0,0,0x400,0,0));
 assert(!fpr_resume(52,0x3b,0x10006,0,0,0x400,0,0));
 assert(!fpr_resume(52,0x38,0x10106,0,0,0x400,0,0));
 for(unsigned i=0;i<4;i++)assert(!fpr_resume(52,0x38,0x10006,1u<<i,0,0x400,0,0));
 assert(!fpr_resume(52,0x38,0x10006,0,1,0x400,0,0));
 const unsigned bad[]={0,0x401,0x402,0x2400};
 for(unsigned i=0;i<4;i++)assert(!fpr_resume(52,0x38,0x10006,0,0,bad[i],0,0));
 assert(!fpr_resume(52,0x38,0x10006,0,0,0x400,0x80000040,0));
 assert(!fpr_resume(52,0x38,0x10006,0,0,0x400,0,0x80000040));
 const unsigned reserved[]={3,5,15,17,22,31,63};
 for(unsigned i=0;i<7;i++)assert(!fpr_resume(52,0x38,0x10006|(UINT64_C(1)<<reserved[i]),0,0,0x400,0,0));
 assert(!fpr_resume(52,0x38,0x10004,0,0,0x400,0,0));
 puts("firmware preemption: preserve RF/RIP, debug/event gates PASS");
}
