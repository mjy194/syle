#include "linux_pic_mask.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool masked(const struct lpic_state*s){return s->slave==255&&(s->master==255||s->master==251);}
void lpic_init(struct lpic_state*s){if(s)*s=(struct lpic_state){255,255,false};}
bool lpic_io(struct lpic_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,
 uint64_t reason,uint64_t q,unsigned length,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||s->failed)return false;
 bool ok=false;
 if(!g||!write||!masked(s)||cpl||reason!=30||q&~UINT64_C(0xffff0048)||
 !canonical(rip)||rip>UINT64_MAX-length||!canonical(rip+length))goto done;
 unsigned port=q>>16;bool input=q&8,immediate=q&64;
 if((port!=0x21&&port!=0xa1)||(immediate?(length!=2):(length!=1||(uint16_t)g[3]!=port)))goto done;
 struct lpic_state next=*s;uint8_t value=port==0x21?s->master:s->slave;
 if(!input){value=(uint8_t)g[0];if(port==0x21)next.master=value;else next.slave=value;if(!masked(&next))goto done;}
 if(!write(ctx,0x681e,rip+length))goto done;
 *s=next;if(input)g[0]=(g[0]&~UINT64_C(255))|value;
 ok=true;
done:if(!ok)s->failed=true;return ok;
}
