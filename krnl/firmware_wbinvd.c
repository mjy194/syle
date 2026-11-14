#include "firmware_wbinvd.h"
static bool canonical(uint64_t v){return (v>>47)==0||(v>>47)==0x1ffff;}
bool fwb_apply(struct fwb_state*s,uint32_t reason,uint64_t q,uint64_t rip,
 unsigned length,unsigned cpl,const uint8_t*instruction,size_t available,
 bool exclusive,bool admitted,void*ctx,bool(*sample)(void*),bool(*maintain)(void*),bool(*write)(void*,uint32_t,uint64_t)){
 if(!exclusive||!s||s->failed)return false;
 if(!admitted||reason!=54||q||length!=2||cpl||!instruction||available<2||
 instruction[0]!=0x0f||instruction[1]!=0x09||!canonical(rip)||rip>UINT64_MAX-2||
 !canonical(rip+2)||!sample||!maintain||!write||!sample(ctx)||!maintain(ctx)||!sample(ctx)||!write(ctx,0x681e,rip+2)){
  s->failed=true;return false;
 }
 return true;
}
