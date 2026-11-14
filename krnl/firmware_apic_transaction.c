#include "firmware_apic_transaction.h"
#include "guest_mmio_mov.h"
static const unsigned regmap[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
enum fat_result fat_mmio_traced(struct fat_state*s,uint64_t g[15],const struct fmm_memory*m,
 const struct fat_exit*e,uint32_t ecx,uint32_t edx,bool exclusive,void*ctx,bool(*write)(void*,uint32_t,uint64_t),struct fat_trace*trace){
 if(trace)*trace=(struct fat_trace){0};
 if(!s||s->failed||!fag_valid(&s->device)||!g||!m||!e||!write||!exclusive||
 !(edx&(1u<<9))||(ecx&(1u<<21))||e->ds_base||e->es_base||e->ss_base||
 e->vectoring||e->entry_event||e->blocking||e->pending_debug||
 (e->rflags&(UINT64_C(1)<<8))||e->dr7!=0x400)return FAT_REJECTED;
 if(trace){trace->stage=1;trace->lvt=s->device.apic.timer.lvt;trace->initial=s->device.apic.timer.initial;trace->armed=s->device.apic.timer.armed;}
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[regmap[i]];
 uint8_t bytes[15];size_t available=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(trace){trace->available=(unsigned)available;trace->stage=2;}
 if(!available||!guest_mmio_mov_decode(bytes,available,e->rip,regs,&plan))return FAT_REJECTED;
 if(trace){trace->stage=3;trace->width=plan.width;trace->reg=plan.reg;trace->write=plan.write;trace->store_value=plan.store_value;}
 if(plan.width!=4||!fmm_validate(m,plan.linear,plan.width,plan.write,e->reason,e->qualification,e->linear,e->physical,
 e->vectoring,UINT64_C(0xfee00000),4096))return FAT_REJECTED;
 if(trace)trace->stage=4;
 struct fag_state staged=s->device;uint32_t value=(uint32_t)plan.store_value;uint64_t newreg=0;
 if(!fag_mmio(&staged,e->physical-UINT64_C(0xfee00000),4,plan.write,&value))return FAT_REJECTED;
 if(!plan.write&&(plan.reg>=16||!guest_mmio_mov_load_value(&plan,regs[plan.reg],value,&newreg)))return FAT_REJECTED;
 if(trace)trace->stage=5;
 if((!plan.write&&plan.reg==4&&!write(ctx,0x681c,newreg))||
  ((e->rflags&(UINT64_C(1)<<16))&&!write(ctx,0x6820,e->rflags&~(UINT64_C(1)<<16)))||
  !write(ctx,0x681e,plan.next_rip)){
  s->failed=true;return FAT_FATAL;
 }
 s->device=staged;
 if(!plan.write&&plan.reg!=4)g[regmap[plan.reg]]=newreg;
 if(trace)trace->stage=6;
 return FAT_COMMITTED;
}

enum fat_result fat_mmio(struct fat_state*s,uint64_t g[15],const struct fmm_memory*m,const struct fat_exit*e,uint32_t ecx,uint32_t edx,bool exclusive,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 return fat_mmio_traced(s,g,m,e,ecx,edx,exclusive,ctx,write,0);
}
