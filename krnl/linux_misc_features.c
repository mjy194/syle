#include "linux_misc_features.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lmf_apply(enum lcp_kind kind,const struct lvp_cpu_features*cpu,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned len,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 struct lvp_cpuid_result one;
 if(!g||!write||cpl||(reason!=31&&reason!=32)||len!=2||(uint32_t)g[2]!=0x140||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2)||
 !lcp_query(kind,cpu,1,0,&one)||!(one.d&(1u<<5))||(one.c&(1u<<3))||one.a!=0x600)return false;
 if(reason==32&&((uint32_t)g[0]||(uint32_t)g[3]))return false;
 if(!write(ctx,0x681e,rip+2))return false;
 if(reason==31)g[0]=g[3]=0;
 return true;
}
