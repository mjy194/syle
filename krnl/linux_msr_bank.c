#include "linux_msr_bank.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool value_ok(uint32_t id,uint64_t x){
 switch(id){
 case 0xc0000100u:case 0xc0000102u:case 0xc0000082u:case 0xc0000083u:return canonical(x);
 case 0xc0000081u:return (uint32_t)x==0;
 case 0xc0000084u:return (x>>32)==0;
 default:return false;
 }
}
bool lmbr_apply(uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,
 unsigned length,uint32_t basic,uint32_t ext,uint64_t fs,struct lvs_state*s,
 void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!gpr||!s||((uintptr_t)s&15)||!write||!s->ready||s->host_dirty||!s->guest_valid||
    cpl||length!=2||(reason!=31&&reason!=32)||!(basic&(1u<<5))||!(ext&(1u<<29))||
    !canonical(rip)||rip>UINT64_MAX-2||!canonical(rip+2))return false;
 uint32_t id=(uint32_t)gpr[2];bool is_fs=id==0xc0000100u;unsigned slot=LVS_MSR_COUNT;
 if(!is_fs){
  if(id!=0xc0000102u&&(id<0xc0000081u||id>0xc0000084u))return false;
  if(id>=0xc0000081u&&id<=0xc0000084u&&!(ext&(1u<<11)))return false;
  for(unsigned i=0;i<LVS_MSR_COUNT;i++)if(lvs_msr_ids[i]==id){slot=i;break;}
  if(slot==LVS_MSR_COUNT)return false;
 }
 uint64_t current=is_fs?fs:s->guest.msr[slot];
 if(!value_ok(id,current))return false;
 uint64_t value=reason==32?((uint64_t)(uint32_t)gpr[3]<<32)|(uint32_t)gpr[0]:current;
 if(!value_ok(id,value))return false;
 if(is_fs&&reason==32&&!write(ctx,0x680e,value))return false;
 if(!write(ctx,0x681e,rip+2))return false;
 /* No fallible work remains. Software state changes commit after RIP. */
 if(reason==32){if(!is_fs)s->guest.msr[slot]=value;}
 else{gpr[0]=(uint32_t)value;gpr[3]=(uint32_t)(value>>32);}
 return true;
}
