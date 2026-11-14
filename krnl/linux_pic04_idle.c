#include "linux_pic04_idle.h"
bool lpi_plan(const struct lpd_domain*d,uint64_t tick,const struct lip_idle*g,uint64_t*out,unsigned*why){
 if(!d||!g||!out||!why||!lpd_valid(d)||d->uart.irq_dirty||d->master.level4!=linux_uart_irq_level(&d->uart.uart)||d->timer.tick!=tick||tick>=UINT64_MAX/1000||g->reason!=52||g->activity>1||g->blocking>2||g->event||g->error||g->length||g->vector||g->owner||d->master.reserved.serial)return false;
 
 uint64_t next=tick+1;unsigned reason;
 if(g->activity!=1)reason=LPI_ACTIVE;
 else if(!(g->flags&512))reason=LPI_IF_CLEAR;
 else if(g->blocking)reason=LPI_BLOCKED;
 else if(d->master.irr)reason=LPI_PENDING;
 else if(d->master.isr)reason=LPI_ISR;
 else if(!d->master.initialized||d->master.phase||d->master.mask_pending||(d->master.imr&1))reason=LPI_MASKED;
 else if(!d->timer.running||!d->timer.mode)reason=LPI_NO_TIMER;
 else{
  const struct pit0*t=&d->timer;uint64_t deadline;
  if(!pit0_next_edge(t,&deadline)){if(!(t->mode4&&t->strobe_done))return false;reason=LPI_NO_TIMER;}
  else if(deadline-tick>65536)reason=LPI_FUTURE_START;
  else {if(deadline>UINT64_MAX/1000)return false;
 next=deadline;reason=LPI_JUMP;}
 }
 *out=next;*why=reason;return true;
}
