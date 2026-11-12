#include "linux_sysenter_msr.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool valid(uint32_t id,uint64_t x){return id==0x174?x<=65535:canonical(x);}
bool lse_apply(uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,
 unsigned length,uint32_t basic,uint32_t ext,void*r,bool(*read)(void*,uint32_t,uint64_t*),
 void*w,bool(*write)(void*,uint32_t,uint64_t)){
 if(!g||!read||!write||cpl||length!=2||(reason!=31&&reason!=32)||
 !(basic&(1u<<5))||!(ext&(1u<<29))||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 uint32_t id=(uint32_t)g[2];if(id<0x174||id>0x176)return false;
 uint32_t field=id==0x174?0x482a:id==0x175?0x6824:0x6826;uint64_t current;
 if(!read(r,field,&current)||!valid(id,current))return false;
 uint64_t value=reason==32?((uint64_t)(uint32_t)g[3]<<32)|(uint32_t)g[0]:current;
 if(!valid(id,value))return false;
 if(reason==32&&!write(w,field,value))return false;
 if(!write(w,0x681e,rip+2))return false;
 if(reason==31){g[0]=(uint32_t)value;g[3]=(uint32_t)(value>>32);}
 return true;
}
