#include "linux_apic_msr.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lams_init(struct lams_state*s,struct guest_apic*a,uint64_t domain,void*c,lams_notify notify){
 if(!s||!a||!domain||!notify||s->ready||s->busy||s->pending||s->failed)return false;
 *s=(struct lams_state){.model=a,.domain=domain,.sink=c,.notify=notify,.ready=true};return true;
}
static enum lams_result notify_pending(struct lams_state*s){
 if(!s->notify(s->sink,s->domain,s->sequence,s->effect.vector))return LAMS_EFFECT_PENDING;
 s->pending=false;s->effect=(struct guest_apic_effect){0};return LAMS_COMMITTED;
}
enum lams_result lams_retry(struct lams_state*s){
 if(!s||!s->ready||s->busy||s->failed)return LAMS_REJECTED;
 if(!s->pending)return LAMS_COMMITTED;
 s->busy=true;enum lams_result r=notify_pending(s);s->busy=false;return r;
}
enum lams_result lams_apply(struct lams_state*s,uint64_t g[15],uint64_t rip,unsigned cpl,
 uint32_t reason,unsigned len,uint64_t now,uint32_t ecx,uint32_t edx,bool exclusive,
 void*c,bool(*write)(void*,uint32_t,uint64_t)){
 if(!s||!s->ready||s->busy||s->pending||s->failed||!g||!write||!exclusive||cpl||
    (reason!=31&&reason!=32)||len!=2||!(edx&(1u<<9))||!canonical(rip)||
    rip>UINT64_MAX-2||!canonical(rip+2))return LAMS_REJECTED;
 uint32_t id=(uint32_t)g[2];uint64_t value=((uint64_t)(uint32_t)g[3]<<32)|(uint32_t)g[0];
 if(id!=0x1b&&(id<0x800||id>0x8ff||!(ecx&(1u<<21))))return LAMS_REJECTED;
 if(id==0x1b&&reason==32&&(value&(1u<<10))&&!(ecx&(1u<<21)))return LAMS_REJECTED;
 s->busy=true;enum lams_result result=LAMS_REJECTED;
 struct guest_apic staged=*s->model;struct guest_apic_effect effect={0};
 if(!guest_apic_poll(&staged,now))goto done;
 if(reason==31){if(!guest_apic_read(&staged,id,now,&value))goto done;}
 else if(!guest_apic_write(&staged,id,value,now,&effect))goto done;
 bool notify=effect.eoi&&effect.level;
 if(notify&&s->sequence==UINT64_MAX)goto done;
 if(!write(c,0x681e,rip+2)){s->failed=true;result=LAMS_FATAL;goto done;}
 *s->model=staged;
 if(reason==31){g[0]=(uint32_t)value;g[3]=(uint32_t)(value>>32);}
 result=LAMS_COMMITTED;
 if(notify){s->effect=effect;s->sequence++;s->pending=true;result=notify_pending(s);}
done:s->busy=false;return result;
}
