#include "linux_profile_activation.h"
bool lpa_before(struct lpa_state*a,const struct lad_config*c,struct lgs_session*s,const struct lvp_cpu_features*h){
 if(!a||a->failed)return false;
 if(!c||!s||!h)goto fail;
 if(!a->active){
  struct lpg_choice choice;if(!lpg_prepare(c,s,h,&choice))goto fail;
  *a=(struct lpa_state){.kind=choice.kind,.active=true,.session=s,.config=*c,.owner=s->memory.owner,
   .cpu=*h,.physical_bits=s->host.caps.physical_bits,.timer_shift=s->host.caps.timer_shift,.apic=c->irq->apic,.io=c->irq->io,.domain=choice.domain,.eptp=s->memory.eptp,.last_time=choice.initial_time,
   .host_ctx=s->hop.context,.vmwrite=s->hop.vmwrite,.read_ctx=s->platform.ctx,.read_field=s->platform.read_field};
  return true;
 }
 if(a->cpu.basic_edx!=h->basic_edx||a->cpu.extended_edx!=h->extended_edx||a->cpu.physical_bits!=h->physical_bits||
 a->physical_bits!=s->host.caps.physical_bits||a->timer_shift!=s->host.caps.timer_shift||a->session!=s||a->config.transactions!=c->transactions||a->config.irq!=c->irq||
 a->config.ioapic!=c->ioapic||a->config.now!=c->now||a->config.clock_ctx!=c->clock_ctx||
 !c->irq||a->domain!=c->irq->domain||a->apic!=c->irq->apic||a->io!=c->irq->io||
 a->owner.context!=s->memory.owner.context||a->owner.map!=s->memory.owner.map||
 a->eptp!=s->memory.eptp||a->host_ctx!=s->hop.context||a->vmwrite!=s->hop.vmwrite||
 a->read_ctx!=s->platform.ctx||a->read_field!=s->platform.read_field||!lpg_runtime(c,s))goto fail;
 uint64_t now;if(!c->now(c->clock_ctx,&now)||now<a->last_time||now<c->irq->apic->timer.last_now)goto fail;
 a->last_time=now;return true;
fail:a->failed=true;return false;
}
