#include "linux_ioapic_mmio.h"
#include <guest_mmio_mov.h>
static const unsigned map[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
bool lio_init(struct lio_state*s,struct gia*d){if(!s||!d||s->ready||s->busy||s->failed)return false;*s=(struct lio_state){.domain=d,.ready=true};return true;}
enum lams_result lio_apply(struct lio_state*s,uint64_t g[15],const struct lmm_memory*m,const struct lamm_exit*e,uint32_t edx,bool exclusive,void*c,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||!s->ready||s->busy||s->failed||!g||!m||!e||!write||!exclusive||!(edx&(1u<<9))||e->ds_base||e->es_base||e->ss_base||!gia_exclusive(s->domain))return LAMS_REJECTED;
 struct gia*live=s->domain;if(live->io->enqueue!=gia_enqueue||live->io->context!=live)return LAMS_REJECTED;
 s->busy=true;enum lams_result result=LAMS_REJECTED;
 uint64_t regs[16];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[map[i]];
 uint8_t bytes[15];size_t available=lmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(!available||!guest_mmio_mov_decode(bytes,available,e->rip,regs,&plan)||plan.width!=4||!lmm_validate(m,plan.linear,plan.width,plan.write,e->reason,e->qualification,e->linear,e->physical,e->vectoring,0xfec00000,4096))goto done;
 struct guest_apic a=*live->apic;struct ioapic io=*live->io;struct gia candidate=*live;
 candidate.apic=&a;candidate.io=&io;io.enqueue=gia_enqueue;io.context=&candidate;
 uint32_t value=(uint32_t)plan.store_value;uint64_t newreg=0;
 if(gia_ioapic_mmio(&candidate,e->physical-0xfec00000,4,plan.write,&value)!=DEVICE_HANDLED)goto done;
 if(!plan.write&&(plan.reg>=16||!guest_mmio_mov_load_value(&plan,regs[plan.reg],value,&newreg)))goto done;
 if((!plan.write&&plan.reg==4&&!write(c,0x681c,newreg))||!write(c,0x681e,plan.next_rip)){s->failed=true;result=LAMS_FATAL;goto done;}
 /* No candidate pointers escape. GIA bookkeeping did not change in MMIO path. */
 io.enqueue=live->io->enqueue;io.context=live->io->context;
 *live->apic=a;*live->io=io;
 if(!plan.write&&plan.reg!=4)g[map[plan.reg]]=newreg;
 result=LAMS_COMMITTED;
 done:s->busy=false;return result;
}
