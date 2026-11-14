#include "firmware_timer_100m_mmio.h"
static unsigned divisor(uint32_t dcr){
 unsigned n=(dcr&3)|((dcr>>1)&4);
 return n==7?1:2u<<n;
}
bool ft100_mmio_init(struct ft100_mmio*s,uint64_t epoch,uint64_t reference,uint64_t max_gap){
 if(!s||s->dcr||!ft100_init(&s->timer,epoch,reference,max_gap))return false;
 s->dcr=0;return true;
}
enum ft100_result ft100_mmio_access(struct ft100_mmio*s,uint64_t epoch,uint64_t reference,
 bool enabled,uint64_t offset,unsigned width,bool write,uint32_t*value){
 if(!s)return FT100_BAD;
 uint32_t current=0;
 enum ft100_result rc=ft100_sample(&s->timer,epoch,reference,&current);
 if(rc!=FT100_OK)return rc;
 if(!enabled){s->timer.terminal=true;return FT100_BAD;}
 if(!value||width!=4||(offset!=0x320&&offset!=0x380&&offset!=0x390&&offset!=0x3e0))return FT100_BAD;
 if(!write){
  uint32_t result=offset==0x320?s->timer.lvt:offset==0x380?s->timer.initial:offset==0x390?current:s->dcr;
  *value=result;return FT100_OK;
 }
 uint32_t v=*value;
 if(offset==0x390)return FT100_BAD;
 if(offset==0x320){
  if((v&~UINT32_C(0x300ff))||(s->timer.armed&&((v^s->timer.lvt)&0x20000)))return FT100_BAD;
  s->timer.lvt=v;return FT100_OK;
 }
 if(offset==0x3e0){
  if((v&~UINT32_C(0xb))||s->timer.armed)return FT100_BAD;
  s->dcr=v;s->timer.divisor=divisor(v);return FT100_OK;
 }
 return ft100_start(&s->timer,epoch,reference,v,s->timer.divisor,s->timer.lvt);
}
