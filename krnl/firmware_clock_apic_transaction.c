#include "firmware_clock_apic_transaction.h"
#include "guest_mmio_mov.h"
#ifdef SYLE_EXPERIMENTAL_DIV2_TO_1
#include "firmware_timer_div2to1.h"
#endif
static const unsigned regmap[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
enum fat_result fct_mmio_traced(struct fct_state*s,uint64_t g[15],const struct fmm_memory*m,
 const struct fat_exit*e,uint32_t ecx,uint32_t edx,bool exclusive,void*ctx,bool(*write)(void*,uint32_t,uint64_t),struct fat_trace*trace){
 if(trace)*trace=(struct fat_trace){0};
 if(!exclusive)return FAT_REJECTED;
 if(s && !s->failed && ft100_sample(&s->clock.timer,s->epoch,s->reference,0)!=FT100_OK) {s->failed=true;return FAT_FATAL;}
 if(!s||s->failed||!fag_valid(&s->device)||!g||!m||!e||!write||!exclusive||
 !(edx&(1u<<9))||(ecx&(1u<<21))||e->ds_base||e->es_base||e->ss_base||
 e->vectoring||e->entry_event||e->blocking||e->pending_debug||
 (e->rflags&(UINT64_C(1)<<8))||e->dr7!=0x400)return FAT_REJECTED;
 if(trace){trace->stage=1;trace->lvt=s->clock.timer.lvt;trace->initial=s->clock.timer.initial;trace->armed=s->clock.timer.armed;}
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[regmap[i]];
 uint8_t bytes[15];size_t available=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(trace){trace->available=(unsigned)available;trace->stage=2;}
 if(!available||!guest_mmio_mov_decode(bytes,available,e->rip,regs,&plan))return FAT_REJECTED;
 if(trace){trace->stage=3;trace->width=plan.width;trace->reg=plan.reg;trace->write=plan.write;trace->store_value=plan.store_value;}
 if(plan.width!=4||!fmm_validate(m,plan.linear,plan.width,plan.write,e->reason,e->qualification,e->linear,e->physical,
 e->vectoring,UINT64_C(0xfee00000),4096))return FAT_REJECTED;
 if(trace)trace->stage=4;
 struct fag_state staged=s->device;uint32_t value=(uint32_t)plan.store_value;uint64_t newreg=0;
 struct ft100_mmio clock=s->clock;
 uint64_t off=e->physical-UINT64_C(0xfee00000);
 bool timer=off==0x320||off==0x380||off==0x390||off==0x3e0;
 if(timer) {
  enum ft100_result r;
#ifdef SYLE_EXPERIMENTAL_DIV2_TO_1
  /* Explicit nondefault virtual-platform transition. Sample instant is the
   * existing transaction reference; do not resample then overwrite old time. */
  if(off==0x3e0&&plan.write&&clock.timer.armed&&(staged.apic.irq.svr&0x100))
   r=ft100_mmio_redivide_2_to_1(&clock,s->epoch,s->reference,true,4,value);
  else
#endif
   r=ft100_mmio_access(&clock,s->epoch,s->reference,!!(staged.apic.irq.svr&0x100),off,4,plan.write,&value);
  if(clock.timer.terminal){s->clock.timer.terminal=true;s->failed=true;return FAT_FATAL;}
  if(r!=FT100_OK)return FAT_REJECTED;
 } else {
  if(off==0xf0&&plan.write&&!(value&0x100)&&s->clock.timer.armed){s->clock.timer.terminal=true;s->failed=true;return FAT_FATAL;}
  if(!fag_mmio(&staged,off,4,plan.write,&value))return FAT_REJECTED;
 }
 if(!plan.write&&(plan.reg>=16||!guest_mmio_mov_load_value(&plan,regs[plan.reg],value,&newreg)))return FAT_REJECTED;
 if(trace)trace->stage=5;
 if((!plan.write&&plan.reg==4&&!write(ctx,0x681c,newreg))||
  ((e->rflags&(UINT64_C(1)<<16))&&!write(ctx,0x6820,e->rflags&~(UINT64_C(1)<<16)))||
  !write(ctx,0x681e,plan.next_rip)){
  s->failed=true;s->clock.timer.terminal=true;return FAT_FATAL;
 }
 s->device=staged;s->clock=clock;
 if(!plan.write&&plan.reg!=4)g[regmap[plan.reg]]=newreg;
 if(trace)trace->stage=6;
 return FAT_COMMITTED;
}

enum fat_result fct_mmio(struct fct_state*s,uint64_t g[15],const struct fmm_memory*m,const struct fat_exit*e,uint32_t ecx,uint32_t edx,bool exclusive,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 return fct_mmio_traced(s,g,m,e,ecx,edx,exclusive,ctx,write,0);
}
