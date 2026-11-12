#include "linux_pic_mask.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool masked(const struct lpic_state*s){return s->slave==255&&(s->master==255||s->master==251);}
static bool valid(const struct lpic_state*s){
 if(!s||s->failed)return false;
 for(unsigned i=0;i<2;i++){const struct lpic_chip*c=&s->chip[i];uint8_t mask=i?s->slave:s->master;
  if((!mask&&!c->started)||(c->phase==1&&(c->vector||c->cascade||c->aeoi))||
  (c->phase==2&&(c->cascade||c->aeoi))||(c->phase==3&&c->aeoi)||c->phase>3||(!c->started&&(c->phase||c->vector||c->cascade||c->aeoi)))return false;
  if(c->started){if(c->phase&&mask)return false;if(c->phase!=1&&(c->vector<32||(c->vector&7)))return false;
   if((c->phase==3||!c->phase)&&c->cascade!=(i?2:4))return false;}
 }
 /* Only ICW-induced zero masks may temporarily violate the cold mask profile. */
 if(!masked(s)&&!((s->master==0&&s->chip[0].started)||(s->slave==0&&s->chip[1].started)))return false;
 return (s->master==0||s->master==255||s->master==251)&&(s->slave==0||s->slave==255);
}
bool lpic_entry_ok(const struct lpic_state*s,uint64_t flags){return valid(s)&&
 (!(s->chip[0].phase||s->chip[1].phase||!masked(s))||!(flags&(1u<<9)));}
void lpic_init(struct lpic_state*s){if(s)*s=(struct lpic_state){.master=255,.slave=255};}
bool lpic_io_flags(struct lpic_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,
 uint64_t reason,uint64_t q,unsigned length,uint64_t flags,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||s->failed)return false;
 bool ok=false;
 if(!g||!write||!lpic_entry_ok(s,flags)||cpl||reason!=30||q&~UINT64_C(0xffff0048)||
 !canonical(rip)||rip>UINT64_MAX-length||!canonical(rip+length))goto done;
 unsigned port=q>>16;bool input=q&8,immediate=q&64;
 if((port!=0x20&&port!=0x21&&port!=0xa0&&port!=0xa1)||(immediate?(length!=2):(length!=1||(uint16_t)g[3]!=port)))goto done;
 struct lpic_state next=*s;unsigned n=port>=0xa0;struct lpic_chip*c=&next.chip[n];uint8_t*mask=n?&next.slave:&next.master;
 uint8_t value=*mask;
 if(!(port&1)){
  /* No OCW/IRR/ISR/EOI fake completion. Only exact cascaded edge ICW1. */
  if(input||(uint8_t)g[0]!=0x11||(flags&(1u<<9)))goto done;
  *c=(struct lpic_chip){.phase=1,.started=true};*mask=0;
 }else if(!input){
  value=(uint8_t)g[0];
  if(c->phase==1){if(value<32||(value&7))goto done;c->vector=value;c->phase=2;}
  else if(c->phase==2){if(value!=(n?2:4))goto done;c->cascade=value;c->phase=3;}
  else if(c->phase==3){if(value!=1&&value!=3)goto done;c->aeoi=value==3;c->phase=0;}
  else { *mask=value;
   /* Slave stays masked; cascade may open only after slave restoration. */
   if(n?value!=255:(value!=255&&(value!=251||next.slave!=255)))goto done;
  }
 }
 if(!lpic_entry_ok(&next,flags)||!write(ctx,0x681e,rip+length))goto done;
 *s=next;if(input)g[0]=(g[0]&~UINT64_C(255))|value;
 ok=true;
done:if(!ok)s->failed=true;return ok;
}
bool lpic_io(struct lpic_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,void*ctx,bool(*w)(void*,uint32_t,uint64_t)){
 return lpic_io_flags(s,g,rip,cpl,reason,q,len,1u<<9,ctx,w);
}
