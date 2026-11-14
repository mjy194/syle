#include "firmware_timer_div2to1.h"
enum ft100_result ft100_mmio_redivide_2_to_1(struct ft100_mmio*s,uint64_t epoch,
 uint64_t reference,bool enabled,unsigned width,uint32_t value){
 if(!s)return FT100_BAD;
 enum ft100_result r=ft100_sample(&s->timer,epoch,reference,0);if(r!=FT100_OK)return r;
 if(!enabled){s->timer.terminal=true;return FT100_BAD;}
 struct ft100*t=&s->timer;
 if(width!=4||value!=0xb||s->dcr||t->divisor!=2||!t->armed||
 t->initial!=UINT32_MAX||t->lvt!=0x30005||t->period_bus!=(uint64_t)UINT32_MAX*2||
 (t->deadline_bus&1))return FT100_BAD;
 /* sample has already admitted monotonic epoch/reference and elapsed*10. */
 uint64_t elapsed=(reference-t->origin)*10;
 if(t->deadline_bus<=elapsed)return FT100_BAD;
 uint64_t remaining=t->deadline_bus-elapsed;
 if((remaining&1)||remaining>t->period_bus)return FT100_BAD;
 uint64_t next=remaining/2;
 if(next>UINT64_MAX-elapsed){t->terminal=true;return FT100_BAD;}
 t->deadline_bus=elapsed+next;t->period_bus=t->initial;t->divisor=1;s->dcr=0xb;
 return FT100_OK;
}
