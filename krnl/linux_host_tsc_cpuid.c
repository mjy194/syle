#include "linux_host_tsc_cpuid.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lht_query(const struct lvp_cpu_features*h,uint32_t leaf,uint32_t sub,struct lvp_cpuid_result*out){
 if(!out)return false;
 struct lvp_cpuid_result r;if(!lvp_cpuid(h,leaf,sub,&r))return false;
 if(leaf==0)r.a=0x15;
 if(leaf==0x15){
  /* Linux6.12 calculates (ECX/1000)*EBX/EAX with uint32 operands.
   * This encoding is exact without truncation or intermediate overflow. */
  r=(struct lvp_cpuid_result){.a=1000,.b=1193182,.c=1000000};
 }
 if(leaf==0x80000007)r.d=1u<<8;
 *out=r;return true;
}
bool lht_apply(const struct lvp_cpu_features*h,uint64_t g[15],uint64_t rip,unsigned cpl,unsigned reason,unsigned len,void*p,bool(*write)(void*,uint32_t,uint64_t)){
 if(!g||!write||(cpl!=0&&cpl!=3)||reason!=10||len!=2||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 struct lvp_cpuid_result r;if(!lht_query(h,(uint32_t)g[0],(uint32_t)g[2],&r)||!write(p,0x681e,rip+2))return false;
 g[0]=r.a;g[1]=r.b;g[2]=r.c;g[3]=r.d;return true;
}
