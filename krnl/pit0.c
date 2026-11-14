#include "pit0.h"
#include <limits.h>
static bool count_ok(uint32_t n,bool mode4){return n>=(mode4?1u:2u)&&n<=65536;}
static bool valid(const struct pit0*s){return s&&(!s->mode4||s->mode)&&
 (!s->running||(s->mode&&count_ok(s->reload,s->mode4)))&&
 (!s->pending||(s->running&&!s->mode4&&count_ok(s->pending,false)))&&
 (!s->latched||s->running)&&(!s->latch_high||s->latched)&&(!s->high_byte||s->mode)&&
 (!s->strobe_done||(s->mode4&&s->running))&&(!s->load_low||(s->mode4&&s->running))&&
 (!s->mode4||!s->running||s->start<=UINT64_MAX-s->reload-1);}

void pit0_init(struct pit0*s){if(s)*s=(struct pit0){0};}
bool pit0_output(const struct pit0*s,bool*v){
 if(!valid(s)||!v)return false;
 if(s->mode4)*v=!s->running||(s->tick<s->start?!s->load_low:s->tick-s->start!=s->reload);
 else *v=!s->running||s->tick<s->start||(s->tick-s->start)%s->reload!=s->reload-1;
 return true;
}
bool pit0_count(const struct pit0*s,uint16_t*v){
 if(!valid(s)||!v||!s->running)return false;
 if(s->mode4)*v=s->tick<s->start?s->load_count:(uint16_t)(s->reload-(uint16_t)(s->tick-s->start));
 else *v=(uint16_t)(s->tick<s->start?s->reload:s->reload-(s->tick-s->start)%s->reload);
 return true;
}
bool pit0_next_edge(const struct pit0*s,uint64_t*out){
 if(!valid(s)||!out||!s->running||!s->mode)return false;
 uint64_t n;
 if(s->mode4){
  if(s->tick<s->start&&s->load_low)n=s->start;
  else {if(s->strobe_done)return false;n=s->start+s->reload+1;}
 }else {if(s->start>UINT64_MAX-s->reload)return false;n=s->start+s->reload;}
 if(n<=s->tick)return false;
 *out=n;return true;
}
static bool add_edges(struct pit0*s,uint64_t n){if(n>UINT64_MAX-s->edges)return false;s->edges+=n;return true;}
static bool advance(struct pit0*s,uint64_t t){
 if(!valid(s)||t<s->tick)return false;
 if(s->mode4&&s->running&&t>=s->start){
  /* Count load is next CLK; terminal count drives OUT low for one CLK.
   * After its rising edge the 16-bit count wraps, but OUT stays high. */
  if(s->tick<s->start&&s->load_low){if(!add_edges(s,1))return false;s->load_low=false;}
  if(!s->strobe_done&&t>=s->start+s->reload+1){if(!add_edges(s,1))return false;s->strobe_done=true;}
 }else if(s->running&&t>=s->start){
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
  if(out==0x34||out==0x38){bool level;if(!pit0_output(&n,&level))return false;if(!level&&!add_edges(&n,1))return false;
   n.mode=true;n.mode4=out==0x38;n.strobe_done=n.load_low=false;n.load_count=0;n.running=false;n.reload=n.pending=0;n.high_byte=n.read_high=n.latched=n.latch_high=false;
  }else if(out==0){if(!n.running||n.read_high)return false;if(!n.latched){if(!pit0_count(&n,&n.latch))return false;n.latched=true;n.latch_high=false;}}
  else return false;
 }else if(port==0x40){
  if(!n.mode)return false;
  if(write){if(!n.high_byte){n.low=out;n.high_byte=true;}else{
    uint32_t reload=((uint32_t)out<<8)|n.low;if(!reload)reload=65536;if(!count_ok(reload,n.mode4))return false;
    n.high_byte=false;
    if(n.mode4){
     bool level;uint16_t count=(uint16_t)reload;
     if(!pit0_output(&n,&level)||(n.running&&!pit0_count(&n,&count))||t>UINT64_MAX-reload-2)return false;
     n.reload=reload;n.pending=0;n.start=t+1;n.running=true;n.strobe_done=false;
     n.load_low=!level;n.load_count=count;
    }else if(n.running){if(t<n.start){n.reload=reload;n.pending=0;}else n.pending=reload;}else{if(t==UINT64_MAX)return false;n.reload=reload;n.start=t+1;n.running=true;}
   }}else if(n.latched){out=(uint8_t)(n.latch>>(n.latch_high?8:0));if(n.latch_high){n.latched=false;n.latch_high=false;}else n.latch_high=true;
  }else {uint16_t count;if(!pit0_count(&n,&count))return false;out=(uint8_t)(count>>(n.read_high?8:0));n.read_high=!n.read_high;}
 }else return false;
 if(!valid(&n))return false;
 *s=n;*v=out;return true;
}
