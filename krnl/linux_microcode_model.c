#include "linux_microcode_model.h"
#include "linux_cpu_commit.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lmc_msr(struct lmc_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned len,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||!g||!write||cpl||(reason!=31&&reason!=32)||len!=2||
    (uint32_t)g[2]!=0x8b||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 if(reason==32&&((uint32_t)g[0]||(uint32_t)g[3]))return false;
 if(!write(ctx,0x681e,rip+2))return false;
 if(reason==32)s->loaded=false;
 else {g[0]=0;g[3]=0;}
 return true;
}
bool lmc_cpuid(struct lmc_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned len,const struct lvp_cpu_features*h,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||!g)return false;
 uint32_t leaf=(uint32_t)g[0];
 if(!lcpu_apply(g,rip,cpl,reason,len,h,ctx,write))return false;
 if(leaf==1)s->loaded=true;
 return true;
}
