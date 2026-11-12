#include "linux_port80.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lp80_io(struct lp80_state*s,const uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||s->failed)return false;
 if(!g||!write||cpl||reason!=30||(q&~UINT64_C(0xffff0040))||(q>>16)!=0x80||
 ((q&64)?len!=2:(len!=1||(uint16_t)g[3]!=0x80))||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len)||s->writes==UINT64_MAX){s->failed=true;return false;}
 struct lp80_state next=*s;next.last=(uint8_t)g[0];next.writes++;
 if(!write(ctx,0x681e,rip+len)){s->failed=true;return false;}
 *s=next;return true;
}
