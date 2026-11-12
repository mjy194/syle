#include "linux_apic_mmio.h"
#include <guest_mmio_mov.h>
static const unsigned map[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
enum lams_result lamm_apply(struct lams_state*s,uint64_t g[15],const struct lmm_memory*m,
 const struct lamm_exit*e,uint64_t now,uint32_t edx,bool exclusive,void*c,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||!s->ready||s->busy||s->pending||s->failed||!g||!m||!e||!write||!exclusive||
    !(edx&(1u<<9))||e->ds_base||e->es_base||e->ss_base||s->model->x2apic)return LAMS_REJECTED;
 s->busy=true;enum lams_result result=LAMS_REJECTED;
 uint64_t x86[16];for(unsigned i=0;i<16;i++)x86[i]=i==4?e->rsp:g[map[i]];
 uint8_t bytes[15];size_t available=lmm_fetch(m,e->rip,bytes);struct guest_mmio_mov plan;
 if(!available||!guest_mmio_mov_decode(bytes,available,e->rip,x86,&plan)||plan.width!=4||
    !lmm_validate(m,plan.linear,plan.width,plan.write,e->reason,e->qualification,e->linear,e->physical,
      e->vectoring,UINT64_C(0xfee00000),4096))goto done;
 struct guest_apic staged=*s->model;struct guest_apic_effect effect={0};
 uint32_t value=(uint32_t)plan.store_value;uint64_t newreg=0;
 if(!guest_apic_poll(&staged,now)||!guest_apic_mmio(&staged,e->physical-UINT64_C(0xfee00000),4,plan.write,&value,now,&effect))goto done;
 if(!plan.write&&(plan.reg>=16||!guest_mmio_mov_load_value(&plan,x86[plan.reg],value,&newreg)))goto done;
 bool notify=effect.eoi&&effect.level;
 if(notify&&s->sequence==UINT64_MAX)goto done;
 if((!plan.write&&plan.reg==4&&!write(c,0x681c,newreg))||!write(c,0x681e,plan.next_rip)){
  s->failed=true;result=LAMS_FATAL;goto done;
 }
 *s->model=staged;
 if(!plan.write&&plan.reg!=4)g[map[plan.reg]]=newreg;
 result=LAMS_COMMITTED;
 if(notify){s->effect=effect;s->sequence++;s->pending=true;}
done:
 s->busy=false;
 if(result==LAMS_COMMITTED&&s->pending)return lams_retry(s);
 return result;
}
