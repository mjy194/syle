#include "linux_cpu_profile.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lcp_query(enum lcp_kind p,const struct lvp_cpu_features*h,uint32_t leaf,uint32_t sub,struct lvp_cpuid_result*out){
 if(!out||(p!=LCP_BASELINE&&p!=LCP_EXPERIMENTAL_APIC))return false;
 struct lvp_cpuid_result r;if(!lvp_cpuid(h,leaf,sub,&r))return false;
 if(p==LCP_EXPERIMENTAL_APIC&&leaf==1){
  /* Single logical processor, initial APIC ID 0. Entirely virtual APIC:
   * no passthrough and no dependency on host x2APIC support. */
  r.b=(r.b&0xffffu)|(1u<<16);r.c|=1u<<21;r.d|=1u<<9;
 }
 *out=r;return true;
}
bool lcp_apply(enum lcp_kind p,const struct lvp_cpu_features*h,uint64_t g[15],uint64_t rip,unsigned cpl,uint32_t reason,unsigned len,void*c,bool(*write)(void*,uint32_t,uint64_t)){
 if(!g||!write||cpl||reason!=10||len!=2||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 struct lvp_cpuid_result r;if(!lcp_query(p,h,(uint32_t)g[0],(uint32_t)g[2],&r))return false;
 if(!write(c,0x681e,rip+2))return false;
 g[0]=r.a;g[1]=r.b;g[2]=r.c;g[3]=r.d;return true;
}
