#include "linux_idle_pit.h"
bool lip_plan(const struct pt0*p,uint64_t tick,const struct lip_idle*g,uint64_t*out){
 if(!p||!g||!out||p->failed||tick>=UINT64_MAX/1000||p->timer.tick!=tick||p->pic.seen!=p->timer.edges||g->reason!=52||g->activity>1||g->blocking>2||g->event||g->error||g->length||g->vector||g->owner)return false;
 struct pt0 copy=*p;if(!pt0_tick(&copy,tick))return false;
 uint64_t next=tick+1;
 const struct pit0*t=&p->timer;const struct pic0*c=&p->pic;
 if(g->activity==1&&(g->flags&512)&&!g->blocking&&t->running&&t->mode&&c->initialized&&!c->phase&&!c->mask_pending&&!(c->imr&1)&&!c->irr&&!c->isr&&!c->reserved){
  uint64_t deadline;
  if(pit0_next_edge(t,&deadline)){
   uint64_t delta=deadline-tick;
   if(delta<=65536){if(deadline>UINT64_MAX/1000)return false;next=deadline;}
  }else if(!(t->mode4&&t->strobe_done))return false;
 }
 *out=next;return true;
}
