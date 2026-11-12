#include "pit0.h"
#include <limits.h>
static bool count_ok(uint32_t n){return n>=2&&n<=65536;}
static bool valid(const struct pit0*s){return s&&(!s->running||(s->mode&&count_ok(s->reload)))&&(!s->pending||(s->running&&count_ok(s->pending)))&&(!s->latched||s->running)&&(!s->latch_high||s->latched)&&(!s->high_byte||s->mode);}
void pit0_init(struct pit0*s){if(s)*s=(struct pit0){0};}
bool pit0_output(const struct pit0*s,bool*v){if(!valid(s)||!v)return false;*v=!s->running||s->tick<s->start||(s->tick-s->start)%s->reload!=s->reload-1;return true;}
bool pit0_count(const struct pit0*s,uint16_t*v){if(!valid(s)||!v||!s->running)return false;*v=(uint16_t)(s->tick<s->start?s->reload:s->reload-(s->tick-s->start)%s->reload);return true;}
static bool add_edges(struct pit0*s,uint64_t n){if(n>UINT64_MAX-s->edges)return false;s->edges+=n;return true;}
static bool advance(struct pit0*s,uint64_t t){
 if(!valid(s)||t<s->tick)return false;
 if(s->running&&t>=s->start){
  uint64_t old=s->tick<s->start?0:(s->tick-s->start)/s->reload;
  uint64_t now=(t-s->start)/s->reload;
  if(s->pending&&now>old){
   /* Old start always denotes the current cycle after each poll. */
   if(s->start>UINT64_MAX-s->reload)return false;
   uint64_t boundary=s->start+s->reload;
   if(!add_edges(s,1))return false;
   s->start=boundary;s->reload=s->pending;s->pending=0;
   uint64_t cycles=(t-s->start)/s->reload;
   if(!add_edges(s,cycles))return false;
   s->start+=cycles*s->reload;
  }else if(now>old){if(!add_edges(s,now-old))return false;s->start+=now*s->reload;}
 }
 s->tick=t;return true;
}
bool pit0_advance(struct pit0*s,uint64_t t){if(!s)return false;struct pit0 n=*s;if(!advance(&n,t))return false;*s=n;return true;}
bool pit0_access(struct pit0*s,uint64_t t,unsigned port,bool write,uint8_t*v){
 if(!s||!v)return false;
 struct pit0 n=*s;uint8_t out=*v;if(!advance(&n,t))return false;
 if(port==0x43){
  if(!write)return false;
  if(out==0x34){bool level;if(!pit0_output(&n,&level))return false;if(!level&&!add_edges(&n,1))return false;
   n.mode=true;n.running=false;n.reload=n.pending=0;n.high_byte=n.read_high=n.latched=n.latch_high=false;
  }else if(out==0){if(!n.running||n.read_high)return false;if(!n.latched){if(!pit0_count(&n,&n.latch))return false;n.latched=true;n.latch_high=false;}}
  else return false;
 }else if(port==0x40){
  if(!n.mode)return false;
  if(write){if(!n.high_byte){n.low=out;n.high_byte=true;}else{
    uint32_t reload=((uint32_t)out<<8)|n.low;if(!reload)reload=65536;if(!count_ok(reload))return false;
    n.high_byte=false;if(n.running){if(t<n.start){n.reload=reload;n.pending=0;}else n.pending=reload;}else{if(t==UINT64_MAX)return false;n.reload=reload;n.start=t+1;n.running=true;}
   }}else if(n.latched){out=(uint8_t)(n.latch>>(n.latch_high?8:0));if(n.latch_high){n.latched=false;n.latch_high=false;}else n.latch_high=true;
  }else {uint16_t count;if(!pit0_count(&n,&count))return false;out=(uint8_t)(count>>(n.read_high?8:0));n.read_high=!n.read_high;}
 }else return false;
 if(!valid(&n))return false;
 *s=n;*v=out;return true;
}
