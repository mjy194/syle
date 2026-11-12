#include "linux_apic_dispatch.h"
#include "linux_guest_session.h"
bool lad_valid(const struct lad_config*c){return c&&c->transactions&&c->irq&&c->now&&
 c->transactions->ready&&!c->transactions->failed&&c->irq->enabled&&
 c->transactions->model==c->irq->apic&&c->transactions->domain==c->irq->domain&&
 c->transactions->sink==c->irq&&c->transactions->notify==gia_notify&&
 c->ioapic&&c->ioapic->ready&&!c->ioapic->failed&&c->ioapic->domain==c->irq;}
static bool rd(struct lgs_session*s,uint32_t f,uint64_t*v){return s&&s->platform.read_field&&s->platform.read_field(s->platform.ctx,f,v);}
static bool ready(const struct lad_config*c,struct lgs_session*s){return lad_valid(c)&&s&&s->prepared&&!s->failed&&
 s->state.ready&&s->state.guest_valid&&!s->state.host_dirty&&s->hop.vmwrite&&gia_exclusive(c->irq);}
static bool ram_read(void*p,uint64_t gpa,void*out,size_t n){
 struct lgs_session*s=p;
 if(!s||!out||!s->memory.prepared||!s->memory.owner.map||!n||gpa>=LGM_RAM_SIZE||n>LGM_RAM_SIZE-gpa)return false;
 uint8_t*b=out;
 for(size_t i=0;i<n;i++){
  uint64_t address=gpa+i,pa=s->memory.pages[address/4096];
  if(!pa||(pa&4095))return false;
  const uint8_t*page=s->memory.owner.map(s->memory.owner.context,pa);
  if(!page)return false;
  b[i]=page[address&4095];
 }
 return true;
}
static bool event_clear(struct lgs_session*s){uint64_t a,b,c,v;return rd(s,0x4016,&a)&&rd(s,0x4018,&b)&&rd(s,0x401a,&c)&&rd(s,0x4408,&v)&&!a&&!b&&!c&&!v;}
enum lams_result lad_mmio(const struct lad_config*c,struct lgs_session*s,uint32_t edx){
 if(!ready(c,s)||!event_clear(s))return LAMS_REJECTED;
 struct lmm_memory m={.ctx=s,.read=ram_read,.ram_size=LGM_RAM_SIZE};struct lamm_exit e={0};
 uint64_t cs,ar,reason,vector,now;
 if(!rd(s,0x0802,&cs)||cs>UINT16_MAX||!rd(s,0x4816,&ar)||ar>UINT32_MAX||
    !rd(s,0x6808,&m.cs_base)||!rd(s,0x6800,&m.cr0)||!rd(s,0x6802,&m.cr3)||
    !rd(s,0x6804,&m.cr4)||!rd(s,0x2806,&m.efer)||!rd(s,0x681e,&e.rip)||
    !rd(s,0x681c,&e.rsp)||!rd(s,0x6400,&e.qualification)||!rd(s,0x640a,&e.linear)||
    !rd(s,0x2400,&e.physical)||!rd(s,0x680c,&e.ds_base)||!rd(s,0x680a,&e.ss_base)||
    !rd(s,0x6806,&e.es_base)||!rd(s,0x4402,&reason)||reason>UINT32_MAX||
    !rd(s,0x4408,&vector)||vector>UINT32_MAX||!c->now(c->clock_ctx,&now))return LAMS_REJECTED;
 m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)cs;e.reason=(uint32_t)reason;e.vectoring=(uint32_t)vector;
 if(e.physical>=UINT64_C(0xfec00000)&&e.physical<UINT64_C(0xfec01000))
  return lio_apply(c->ioapic,s->native.gpr,&m,&e,edx,true,s->hop.context,s->hop.vmwrite);
 return lamm_apply(c->transactions,s->native.gpr,&m,&e,now,edx,true,s->hop.context,s->hop.vmwrite);
}
enum lams_result lad_msr(const struct lad_config*c,struct lgs_session*s,uint32_t ecx,uint32_t edx){
 if(!ready(c,s)||!event_clear(s))return LAMS_REJECTED;
 uint64_t rip,cs,reason,len,now;
 if(!rd(s,0x681e,&rip)||!rd(s,0x0802,&cs)||cs>UINT16_MAX||(cs&3)||
    !rd(s,0x4402,&reason)||reason>UINT32_MAX||!rd(s,0x440c,&len)||len!=2||!c->now(c->clock_ctx,&now))return LAMS_REJECTED;
 return lams_apply(c->transactions,s->native.gpr,rip,0,(uint32_t)reason,2,now,ecx,edx,true,s->hop.context,s->hop.vmwrite);
}
