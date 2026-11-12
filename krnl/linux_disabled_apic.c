#include "linux_disabled_apic.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lda_read(enum lcp_kind profile,uint32_t ecx,uint32_t edx,uint64_t g[15],uint64_t rip,unsigned cpl,uint32_t reason,unsigned len,void*c,bool(*write)(void*,uint32_t,uint64_t)){
 if(!g||!write||profile!=LCP_BASELINE||!(edx&(1u<<5))||(edx&(1u<<9))||(ecx&(1u<<21))||
 (uint32_t)g[2]!=0x1b||cpl||reason!=31||len!=2||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 if(!write(c,0x681e,rip+2))return false;
 g[0]=0xfee00100;g[3]=0;return true;
}
