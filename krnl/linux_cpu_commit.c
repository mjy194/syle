#include "linux_cpu_commit.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lcpu_host_valid(uint32_t max,uint32_t b,uint32_t c,uint32_t d,uint32_t ext,const struct lvp_cpu_features*h){
 return max>=1&&b==0x756e6547&&c==0x6c65746e&&d==0x49656e69&&ext>=0x80000008&&lvp_cpu_supported(h);
}
bool lcpu_apply(uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned length,const struct lvp_cpu_features*h,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!gpr||!write||cpl||reason!=10||length!=2||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 struct lvp_cpuid_result r;
 if(!lvp_cpuid(h,(uint32_t)gpr[0],(uint32_t)gpr[2],&r))return false;
 if(!write(ctx,0x681e,rip+2))return false;
 gpr[0]=r.a;gpr[1]=r.b;gpr[2]=r.c;gpr[3]=r.d;
 return true;
}
