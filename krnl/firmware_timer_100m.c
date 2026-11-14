#include "firmware_timer_100m.h"
static bool valid_div(unsigned d){return d && d<=128 && !(d&(d-1));}
static bool admit(struct ft100*s,uint64_t e,uint64_t r) {
 if(!s||!s->ready||s->terminal)return false;
 if(e!=s->epoch||r<s->last||r-s->last>s->max_gap||r<s->origin||r-s->origin>UINT64_MAX/10) {
  s->terminal=true;return false;
 }
 return true;
}
bool ft100_init(struct ft100*s,uint64_t e,uint64_t r,uint64_t gap) {
 if(!s||s->ready||s->terminal||!e||!gap)return false;
 *s=(struct ft100){.epoch=e,.origin=r,.last=r,.max_gap=gap,.lvt=0x10000,.divisor=2,.ready=true};return true;
}
enum ft100_result ft100_sample(struct ft100*s,uint64_t e,uint64_t r,uint32_t*out) {
 if(!admit(s,e,r))return FT100_BAD;
 uint64_t elapsed=(r-s->origin)*10;
 /* Due event takes precedence over invalid optional-output use. */
 if(s->armed&&elapsed>=s->deadline_bus&&!(s->lvt&0x10000)) {s->terminal=true;return FT100_STOP;}
 struct ft100 n=*s;n.last=r;
 if(n.armed&&elapsed>=n.deadline_bus) {
  if(n.lvt&0x20000) {
   uint64_t k=(elapsed-n.deadline_bus)/n.period_bus+1;
   if(k>(UINT64_MAX-n.deadline_bus)/n.period_bus){s->terminal=true;return FT100_BAD;}
   n.deadline_bus+=k*n.period_bus;
  } else n.armed=false;
 }
 uint64_t remain=n.armed?n.deadline_bus-elapsed:0;
 uint64_t count=remain/n.divisor+(remain%n.divisor!=0);
 if(count>UINT32_MAX){s->terminal=true;return FT100_BAD;}
 *s=n;if(out)*out=(uint32_t)count;return FT100_OK;
}
enum ft100_result ft100_start(struct ft100*s,uint64_t e,uint64_t r,uint32_t count,unsigned div,uint32_t lvt) {
 /* Observe any old event BEFORE cancellation/configuration. */
 enum ft100_result rc=ft100_sample(s,e,r,0);if(rc!=FT100_OK)return rc;
 if(!valid_div(div)||(lvt&~UINT32_C(0x300ff)))return FT100_BAD;
 uint64_t bus=(r-s->origin)*10,period=(uint64_t)count*div;
 if(period>UINT64_MAX-bus){s->terminal=true;return FT100_BAD;}
 s->initial=count;s->divisor=div;s->lvt=lvt;s->period_bus=period;
 s->armed=count!=0;s->deadline_bus=count?bus+period:0;
 return FT100_OK;
}
bool ft100_entry_before(const struct ft100*s,uint64_t latest) {
 if(!s||!s->ready||s->terminal||latest<s->last||latest-s->last>s->max_gap||latest<s->origin||latest-s->origin>UINT64_MAX/10)return false;
 return !s->armed||(latest-s->origin)*10<s->deadline_bus;
}
