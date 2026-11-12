#include "linux_pit2.h"
static bool advance(struct lpit2*p,uint64_t tick){
 if(!p||p->failed||tick<p->ticks||tick>UINT64_MAX/1000)return false;
 uint64_t elapsed=tick-p->ticks;p->ticks=tick;
 if(p->programmed&&!p->waiting){
  if(p->loading&&elapsed){p->count=p->reload;p->loading=false;elapsed--;}
  if(!p->loading&&p->gate&&elapsed){
   if(elapsed>=p->count)p->out=true;
   p->count=(uint32_t)((p->count-(elapsed&65535))&65535);
  }
 }return true;
}
bool lpit2_init(struct lpit2*p,uint64_t tick){if(!p||tick>UINT64_MAX/1000)return false;*p=(struct lpit2){.ticks=tick};return true;}
bool lpit2_access(struct lpit2*p,uint16_t port,bool write,uint8_t*v,uint64_t tick){
 if(!p||!v)return false;
 struct lpit2 n=*p;uint8_t value=*v;
 if(!advance(&n,tick))return false;
 if(port==0x61){
  if(write){if(value&~0x23u)return false;n.gate=!!(value&1);n.speaker=!!(value&2);}
  else value=(uint8_t)(n.gate|(n.speaker<<1)|(n.out<<5));
 }else if(port==0x43){
  if(!write||value!=0xb0)return false;
  n.programmed=true;n.waiting=true;n.loading=false;n.out=false;n.write_high=false;n.read_high=false;
 }else if(port==0x42){
  if(!n.programmed)return false;
  if(write){
   if(!n.write_high){n.low=value;n.write_high=true;n.waiting=true;n.loading=false;n.out=false;}
   else {n.reload=((uint32_t)value<<8)|n.low;if(!n.reload)n.reload=65536;
    n.write_high=false;n.waiting=false;n.loading=true;n.out=false;}
  }else {if(n.waiting)return false;value=(uint8_t)(n.count>>(n.read_high?8:0));n.read_high=!n.read_high;}
 }else return false;
 *p=n;*v=value;return true;
}
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool span(uint64_t rip,unsigned n){return canonical(rip)&&rip<=UINT64_MAX-n&&canonical(rip+n);}
enum lpit_result lpit2_io(struct lpit2*p,uint64_t g[15],uint64_t rip,unsigned cpl,uint32_t reason,uint64_t q,unsigned len,uint64_t tick,void*c,bool(*w)(void*,uint32_t,uint64_t)){
 if(!p||!g||!w||p->failed||cpl||reason!=30||(q&~UINT64_C(0xffff0048)))return LPIT_REJECTED;
 bool immediate=!!(q&64),write=!(q&8);unsigned port=(unsigned)(q>>16);
 if(len!=(immediate?2u:1u)||!span(rip,len)||(!immediate&&(uint16_t)g[3]!=port)||
 (immediate&&port>255))return LPIT_REJECTED;
 struct lpit2 next=*p;uint8_t v=(uint8_t)g[0];if(!lpit2_access(&next,(uint16_t)port,write,&v,tick))return LPIT_REJECTED;
 if(!w(c,0x681e,rip+len)){p->failed=true;return LPIT_FATAL;}
 *p=next;if(!write)g[0]=(g[0]&~UINT64_C(255))|v;return LPIT_COMMITTED;
}
enum lpit_result lpit2_rdtsc(struct lpit2*p,uint64_t g[15],uint64_t rip,unsigned cpl,uint32_t reason,unsigned len,uint64_t tick,void*c,bool(*w)(void*,uint32_t,uint64_t)){
 if(!p||!g||!w||p->failed||cpl||reason!=16||len!=2||!span(rip,len))return LPIT_REJECTED;
 struct lpit2 next=*p;if(!advance(&next,tick))return LPIT_REJECTED;
 if(!w(c,0x681e,rip+2)){p->failed=true;return LPIT_FATAL;}
 *p=next;uint64_t tsc=tick*1000;g[0]=(uint32_t)tsc;g[3]=(uint32_t)(tsc>>32);return LPIT_COMMITTED;
}
