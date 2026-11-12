#include "../linux_cpu_profile.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned writes;static bool fail;
static bool wr(void*c,uint32_t f,uint64_t v){(void)c;assert(f==0x681e&&v==0x1002);writes++;return !fail;}
int main(void){struct lvp_cpu_features h={.basic_edx=UINT32_MAX,.extended_edx=UINT32_MAX,.physical_bits=32};struct lvp_cpuid_result a,b;
 for(unsigned i=0;i<256;i++)for(unsigned sub=0;sub<3;sub++){uint32_t leaf=i<128?i:0x80000000u+i-128;assert(lvp_cpuid(&h,leaf,sub,&a)&&lcp_query(LCP_BASELINE,&h,leaf,sub,&b)&&!memcmp(&a,&b,sizeof a));assert(lcp_query(LCP_EXPERIMENTAL_APIC,&h,leaf,sub,&b));if(leaf==1){assert(b.a==a.a&&b.b==(1u<<16)&&b.c==(a.c|(1u<<21))&&b.d==(a.d|(1u<<9)));assert(!(b.c&((1u<<24)|(1u<<26)|(1u<<27)|(1u<<28)|(1u<<17))));}else assert(!memcmp(&a,&b,sizeof a));}
 uint64_t g[15]={0};g[0]=0xffff00000001ull;g[2]=0xffff00000000ull;assert(lcp_apply(LCP_EXPERIMENTAL_APIC,&h,g,0x1000,0,10,2,0,wr)&&g[2]==(1u<<21)&&(g[3]&(1u<<9))&&g[1]==0x10000);
 g[0]=1;uint64_t before[15];memcpy(before,g,sizeof g);fail=true;assert(!lcp_apply(LCP_EXPERIMENTAL_APIC,&h,g,0x1000,0,10,2,0,wr)&&!memcmp(g,before,sizeof g));fail=false;unsigned old=writes;
 assert(!lcp_apply(LCP_EXPERIMENTAL_APIC,&h,g,0x1000,3,10,2,0,wr));assert(!lcp_apply(LCP_EXPERIMENTAL_APIC,&h,g,0x1000,0,48,2,0,wr));assert(!lcp_apply(LCP_EXPERIMENTAL_APIC,&h,g,0x1000,0,10,3,0,wr));assert(!lcp_apply(LCP_EXPERIMENTAL_APIC,&h,g,0x7fffffffffffull,0,10,2,0,wr));assert(!lcp_query((enum lcp_kind)2,&h,1,0,&a));assert(old==writes);
 puts("linux_cpu_profile: unchanged baseline/limited virtual APIC definition and atomic CPUID PASS (host only, no activation)");
}
