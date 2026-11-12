#include "linux_debug_disabled.h"
#include "linux_vcpu_policy.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool ldd_apply(struct ldd_state*d,struct lgs_session*s,uint64_t rip,uint64_t q,unsigned len){
 if(!d||d->failed)return false;
 bool ok=false;
 if(!s||!s->platform.read_field||!s->hop.vmwrite||len<3||len>15||!canonical(rip)||
 rip>UINT64_MAX-len||!canonical(rip+len)||(q&~UINT64_C(0xf17)))goto done;
 unsigned dr=q&7,reg=(q>>8)&15;bool read=!!(q&16);
 if(dr==4||dr==5)goto done;
 for(unsigned i=0;i<4;i++)if(!canonical(d->address[i]))goto done;
 const uint32_t fields[]={0x4402,0x4002,0x681a,0x2802,0x6820,0x6804,0x6800,0x0802,0x4816,0x2806,0x681c};
 uint64_t v[11];for(unsigned i=0;i<11;i++)if(!s->platform.read_field(s->platform.ctx,fields[i],&v[i]))goto done;
 if(v[0]!=29||v[1]>UINT32_MAX||!(v[1]&(1u<<23))||v[2]!=0x400||v[3]||
 (v[4]&((1u<<8)|(1u<<17)))||!lvp_guest_cr4(v[5]&~UINT64_C(0x2000))||!(v[5]&0x2000)||
 !lvp_guest_cr0(v[6])||v[7]>65535||(v[7]&7)||!v[7]||(v[8]&~UINT64_C(1))!=0xa09a||
 (v[9]&0x500)!=0x500)goto done;
 static const unsigned map[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};
 uint64_t value=read?(dr<4?d->address[dr]:dr==6?UINT64_C(0xffff0ff0):0x400):
  reg==4?v[10]:s->native.gpr[map[reg]];
 if(!read){
  if(dr<4&&!canonical(value))goto done;
  if(dr==6&&value!=0&&value!=0xffff0ff0u)goto done;
  if(dr==7&&value!=0&&value!=0x400)goto done;
 }
 if(read&&reg==4&&!s->hop.vmwrite(s->hop.context,0x681c,value))goto done;
 if(!s->hop.vmwrite(s->hop.context,0x681e,rip+len))goto done;
 if(read){if(reg!=4)s->native.gpr[map[reg]]=value;}
 else if(dr<4)d->address[dr]=value;
 ok=true;
done:if(!ok)d->failed=true;return ok;
}
