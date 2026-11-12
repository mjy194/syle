#include "linux_profile_gate.h"
#include "linux_guest_session.h"
#include "linux_acpi_reduced_install.h"
static bool rd(struct lgs_session*s,uint32_t f,uint64_t*out){return s->platform.read_field&&s->platform.read_field(s->platform.ctx,f,out);}
static bool eq(struct lgs_session*s,uint32_t f,uint64_t expected){uint64_t v;return rd(s,f,&v)&&v==expected;}
static bool ctl(struct lgs_session*s,uint32_t f,uint32_t need,uint32_t allow){uint64_t v;return rd(s,f,&v)&&v<=UINT32_MAX&&(v&need)==need&&!(v&~(uint64_t)allow);}
static bool pa(uint64_t a,unsigned bits){return a&&!(a&4095)&&bits>=32&&bits<=52&&!(a>>bits);}
static const uint8_t*table(struct lgs_session*s,unsigned n){uint64_t a=s->memory.tables[n];return pa(a,s->host.caps.physical_bits)?s->memory.owner.map(s->memory.owner.context,a):0;}
static uint64_t u64(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static bool ept(struct lgs_session*s){
 struct linux_guest_memory*m=&s->memory;
 if(m->eptp!=(m->tables[0]|0x1e)||s->host.eptp!=m->eptp||!eq(s,0x201a,m->eptp))return false;
 for(unsigned t=0;t<LGM_TABLE_COUNT;t++){
  const uint8_t*p=table(s,t);if(!p)return false;
  for(unsigned i=0;i<512;i++){
   uint64_t expected=0;
   if(t<2&&i==0){if(!pa(m->tables[t+1],s->host.caps.physical_bits))return false;expected=m->tables[t+1]|7;}
   else if(t==2&&i<LGM_PT_COUNT){if(!pa(m->tables[3+i],s->host.caps.physical_bits))return false;expected=m->tables[3+i]|7;}
   else if(t>=3){uint64_t a=m->pages[(t-3)*512+i];if(!pa(a,s->host.caps.physical_bits))return false;expected=a|0x37;}
   if(u64(p+8*i)!=expected)return false;
  }
 }
 /* Exact root/PDPT unused zeros include both FEC and FEE windows. */
 return true;
}
static bool ram_read(void*p,uint64_t gpa,void*out,size_t n){
 struct lgs_session*s=p;uint8_t*b=out;
 if(!n||gpa>=LGM_RAM_SIZE||n>LGM_RAM_SIZE-gpa)return false;
 for(size_t i=0;i<n;i++){
  uint64_t a=s->memory.pages[(gpa+i)/4096];if(!pa(a,s->host.caps.physical_bits))return false;
  const uint8_t*page=s->memory.owner.map(s->memory.owner.context,a);if(!page)return false;b[i]=page[(gpa+i)&4095];
 }return true;
}
static bool devices(const struct lad_config*c){
 const struct gia*d=c->irq;const struct guest_apic*a=d->apic;const struct lapic_timer*t=&a->timer;
 if(!gia_exclusive(d)||d->generation||d->token||d->receipt_sequence||d->receipt_vector||
    c->transactions->busy||c->transactions->pending||c->transactions->sequence||
    c->ioapic->busy||a->x2apic||a->irq.base!=0xfee00d00||a->irq.svr!=0xff||a->irq.tpr||
    a->ldr||a->dfr!=UINT32_MAX||a->icr||t->lvt!=0x10000||t->divide||t->initial||t->deadline||
    t->period||t->last_expirations||t->armed||!t->periodic_supported||!t->guest_profile||
    !d->io->guest_profile||d->io->selector||d->io->enqueue!=gia_enqueue||d->io->context!=d)return false;
 for(unsigned i=0;i<8;i++)if(a->irq.irr[i]||a->irq.isr[i]||a->tmr[i]||a->pending_level[i])return false;
 for(unsigned i=0;i<6;i++)if(a->lvt[i]!=0x10000)return false;
 for(unsigned i=0;i<IOAPIC_PINS;i++)if(d->io->pins[i].low!=0x10000||d->io->pins[i].asserted||d->io->pins[i].remote_irr)return false;
 return true;
}
bool lpg_prepare(const struct lad_config*c,struct lgs_session*s,const struct lvp_cpu_features*h,struct lpg_choice*out){
 if(!out||!lad_valid(c)||!devices(c)||!lvp_cpu_supported(h)||!s||!s->prepared||s->failed||!s->enabled||
    !s->host.ready||!s->host.active||s->host.quarantined||!s->state.ready||!s->state.guest_valid||s->state.host_dirty||
    s->exits||s->native.resume||!s->memory.prepared||!s->memory.owner.map||s->memory.plan.ram_size!=LGM_RAM_SIZE||
    s->memory.plan.params_addr!=0x90000||s->host.caps.timer_shift>20||
    s->host.owner.context!=s->hop.context||s->host.owner.vmwrite!=s->hop.vmwrite)return false;
 const uint32_t pri=(1u<<7)|(1u<<9)|(1u<<10)|(1u<<11)|(1u<<12)|(1u<<15)|(1u<<16)|(1u<<19)|(1u<<20)|(1u<<23)|(1u<<24)|(1u<<29)|(1u<<30)|(1u<<31);
 const uint32_t en=(1u<<2)|(1u<<9)|(1u<<14)|(1u<<15),ex=(1u<<2)|(1u<<9)|(15u<<18);
 if(!ctl(s,0x4000,0x49,0x5f)||!ctl(s,0x4002,pri,pri|0x0401e172u)||!eq(s,0x401e,0x42)||
    !ctl(s,0x4012,en,en|0x11ffu)||!ctl(s,0x400c,ex,ex|0x36dffu)||!eq(s,0x4004,UINT32_MAX)||
    !eq(s,0x4006,0)||!eq(s,0x4008,0)||!eq(s,0x400a,0)||!eq(s,0x6000,UINT64_MAX)||!eq(s,0x6002,UINT64_MAX)||!eq(s,0x400e,0)||!eq(s,0x4010,0)||!eq(s,0x4014,0)||
    !eq(s,0x4016,0)||!eq(s,0x4018,0)||!eq(s,0x401a,0)||
    !eq(s,0x482e,UINT64_C(1)<<(28-s->host.caps.timer_shift))||
    !eq(s,0x681e,s->memory.entry.rip)||!eq(s,0x681c,s->memory.entry.rsp)||s->native.gpr[4]!=0x90000||!ept(s))return false;
 struct lai_ops ops={.context=s,.read=ram_read};
 if(!lar_verify_owned(&ops,LGM_RAM_SIZE,0x90000))return false;
 uint64_t time;if(!c->now(c->clock_ctx,&time)||time<c->irq->apic->timer.last_now)return false;
 *out=(struct lpg_choice){LCP_EXPERIMENTAL_APIC,c->irq->domain,time};return true;
}
