#include "linux_invlpg.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool linv_apply(struct lgs_session*s,uint64_t rip,uint64_t linear,unsigned length){
 if(!s||!s->memory.prepared||!s->host.active||!s->host.ready||s->host.quarantined||
 !s->platform.read_field||!s->hop.invept||!s->hop.vmwrite||length<3||length>15||
 !canonical(linear)||!canonical(rip)||rip>UINT64_MAX-length||!canonical(rip+length)||
 !s->memory.tables[0]||(s->memory.tables[0]&4095)||
 s->memory.eptp!=(s->memory.tables[0]|0x1e)||s->host.eptp!=s->memory.eptp)return false;
 const uint32_t fields[]={0x4402,0x4002,0x401e,0x201a,0x6800,0x6804,0x2806,0x0802,0x4816,0x4012};
 uint64_t v[10];for(unsigned i=0;i<10;i++)if(!s->platform.read_field(s->platform.ctx,fields[i],&v[i]))return false;
 if(v[0]!=14||v[1]>UINT32_MAX||(v[1]&0x80000200u)!=0x80000200u||v[2]!=0x42||v[3]!=s->memory.eptp||
 (v[4]&0x80000031u)!=0x80000031u||(v[4]&~UINT64_C(0x8005003b))||
 (v[5]&0x2020)!=0x2020||(v[5]&~UINT64_C(0x26b0))||
 (v[6]&0x500)!=0x500||v[7]>65535||(v[7]&7)||!v[7]||
 (v[8]&~UINT64_C(1))!=0xa09a||v[9]>UINT32_MAX||!(v[9]&(1u<<9)))return false;
 /* INVEPT invalidates owned EPT/combined caches, not linear caches alone.
  * With VPID disabled, the following VM entry supplies the linear flush.
  * Never INVLPG the guest operand in the host address space. */
 if(!s->hop.invept(s->hop.context,s->memory.eptp))return false;
 return s->hop.vmwrite(s->hop.context,0x681e,rip+length);
}
