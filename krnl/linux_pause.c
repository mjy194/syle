#include "linux_pause.h"
#include "linux_vcpu_policy.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lpause_apply(struct lgs_session*s,uint64_t rip,unsigned len){
 if(!s||!s->platform.read_field||!s->hop.vmwrite||len!=2||!canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 const uint32_t fields[]={0x4402,0x4002,0x401e,0x0802,0x4816,0x6800,0x6804,0x2806,0x4012};
 uint64_t v[9];for(unsigned i=0;i<9;i++)if(!s->platform.read_field(s->platform.ctx,fields[i],&v[i]))return false;
 if(v[0]!=40||v[1]>UINT32_MAX||(v[1]&0xc0000000u)!=0xc0000000u||v[2]!=0x42||
 !v[3]||v[3]>65535||(v[3]&7)||(v[4]&~UINT64_C(1))!=0xa09a||!lvp_guest_cr0(v[5])||
 !(v[6]&0x2000)||!lvp_guest_cr4(v[6]&~UINT64_C(0x2000))||(v[7]&0x500)!=0x500||
 v[8]>UINT32_MAX||!(v[8]&(1u<<9)))return false;
 return s->hop.vmwrite(s->hop.context,0x681e,rip+2);
}
