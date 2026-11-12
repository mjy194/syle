#include "pit0_transaction.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool pt0_init(struct pt0*s,uint64_t domain){if(!s||!domain)return false;*s=(struct pt0){0};pit0_init(&s->timer);return pic0_init(&s->pic,domain,0);}
static bool sync(struct pt0*s,uint64_t tick){return pit0_advance(&s->timer,tick)&&pic0_edges(&s->pic,s->pic.domain,s->timer.edges);}
bool pt0_tick(struct pt0*s,uint64_t tick){if(!s||s->failed)return false;struct pt0 n=*s;if(!sync(&n,tick))return false;*s=n;return true;}
bool pt0_io(struct pt0*s,uint64_t g[15],uint64_t rip,unsigned cpl,uint32_t reason,uint64_t q,unsigned len,uint64_t tick,void*c,bool(*w)(void*,uint32_t,uint64_t)){
 if(!s||s->failed||!g||!w||cpl||reason!=30||(q&~UINT64_C(0xffff0048)))return false;
 bool input=q&8,immediate=q&64;unsigned port=q>>16;
 if((port!=0x40&&port!=0x43)||len!=(immediate?2u:1u)||(!immediate&&(uint16_t)g[3]!=port)||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len))return false;
 struct pt0 n=*s;uint8_t value=(uint8_t)g[0];
 if(!sync(&n,tick)||!pit0_access(&n.timer,tick,port,!input,&value)||!pic0_edges(&n.pic,n.pic.domain,n.timer.edges))return false;
 if(!w(c,0x681e,rip+len)){s->failed=true;return false;}
 *s=n;if(input)g[0]=(g[0]&~UINT64_C(255))|value;return true;
}
