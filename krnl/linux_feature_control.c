#include "linux_feature_control.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lfc_read(enum lcp_kind kind,const struct lvp_cpu_features*cpu,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned len,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 struct lvp_cpuid_result one,seven;
 if(!g||!write||cpl||reason!=31||len!=2||(uint32_t)g[2]!=0x3a||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2)||
 !lcp_query(kind,cpu,1,0,&one)||!lcp_query(kind,cpu,7,0,&seven)||!(one.d&(1u<<5))||(seven.c&(1u<<30))||(one.c&((1u<<5)|(1u<<6)))||(seven.b&(1u<<2)))return false;
 if(!write(ctx,0x681e,rip+2))return false;
 g[0]=1;g[3]=0;return true;
}
