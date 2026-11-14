#include "firmware_platform_id.h"
bool fpid_read(uint32_t signature,uint32_t msr,uint64_t*value){
 if(!value||signature!=UINT32_C(0x0600)||msr!=UINT32_C(0x17))return false;
 *value=0;return true;
}

static bool canonical(uint64_t v){return (v>>47)==0||(v>>47)==0x1ffff;}
bool fpid_apply(struct fpid_state*s,uint32_t signature,bool intel_vendor,
 uint64_t g[15],uint64_t rip,unsigned length,unsigned cpl,uint32_t reason,
 const uint8_t*instruction,size_t available,bool exclusive,void*ctx,
 bool(*write)(void*,uint32_t,uint64_t)){
 if(!exclusive||!s||s->failed)return false;
 uint64_t value;
 if(!g||!intel_vendor||!write||!instruction||available<2||length!=2||cpl||reason!=31||
 instruction[0]!=0x0f||instruction[1]!=0x32||!canonical(rip)||rip>UINT64_MAX-2||
 !canonical(rip+2)||!fpid_read(signature,(uint32_t)g[2],&value)||!write(ctx,0x681e,rip+2)){
  s->failed=true;return false;
 }
 g[0]=(uint32_t)value;g[3]=(uint32_t)(value>>32);return true;
}
