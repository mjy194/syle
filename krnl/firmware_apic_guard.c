#include "firmware_apic_guard.h"
#define MASK (UINT32_C(1)<<16)
bool fag_valid(const struct fag_state*s){
 if(!s||!s->ready||s->apic.x2apic||s->apic.irq.base!=UINT64_C(0xfee00d00)||
 s->apic.icr||s->apic.timer.armed||s->apic.timer.initial||s->apic.timer.deadline||
 s->apic.timer.period||s->apic.timer.last_now||s->apic.timer.last_expirations||
 (s->apic.timer.lvt&~UINT32_C(0x300ff)))return false;
 for(unsigned i=0;i<6;i++)if(!(s->apic.lvt[i]&MASK)&&!(i==3&&s->lint0_extint_deasserted&&s->apic.lvt[i]==0x700)&&!(i==4&&s->lint1_nmi_deasserted&&s->apic.lvt[i]==0x400))return false;
 for(unsigned i=0;i<8;i++)if(s->apic.irq.irr[i]||s->apic.irq.isr[i]||s->apic.tmr[i]||s->apic.pending_level[i])return false;
 return true;
}
bool fag_lint0_quiet_enable(struct fag_state*s){
 if(!fag_valid(s)||s->lint0_extint_deasserted||s->apic.lvt[3]!=MASK)return false;
 s->lint0_extint_deasserted=true;return true;
}
bool fag_host_events_exit(uint64_t pin){return !(pin>>32)&&(pin&9)==9;}
bool fag_lint1_quiet_enable(struct fag_state*s){
 if(!fag_valid(s)||s->lint1_nmi_deasserted||s->apic.lvt[4]!=MASK)return false;
 s->lint1_nmi_deasserted=true;return true;
}
bool fag_init(struct fag_state*s){
 if(!s||s->ready)return false;
 struct fag_state n={.ready=true};guest_apic_init_mode(&n.apic,0,false);
 if(!fag_valid(&n))return false;
 *s=n;return true;
}
bool fag_base(struct fag_state*s,bool write,uint64_t*v){
 if(!fag_valid(s)||!v)return false;
 if(write)return *v==UINT64_C(0xfee00900); /* Fixed xAPIC, no mode switch. */
 *v=UINT64_C(0xfee00900);return true;
}
bool fag_mmio(struct fag_state*s,uint64_t off,unsigned width,bool write,uint32_t*v){
 if(!fag_valid(s)||!v)return false;
 /* Reject event-generating writes BEFORE touching even a staged model.
  * Initial-count zero means a genuinely stopped timer, not frozen time. */
 if(write&&((off==0x300)||(off==0x310)||(off==0x3f0)||
 (off==0x380&&*v)))return false;
 struct fag_state n=*s;uint32_t value=*v;struct guest_apic_effect effect={0};
 /* Idle LVT programming cannot produce an event: all counter state is zero
  * under fag_valid. Low vectors are register values only, never delivered.
  * Nonzero initial-count remains rejected above, even masked. */
 /* P6-shaped firmware SVR: low nibble is fixed ones. Spurious-vector
  * programming is distinct from accepting an external interrupt vector.
  * No source/events are permitted by fag_valid; delivery remains unsupported. */
 if(write && off==0x350 && value==0x700) {
  if(width!=4||!s->lint0_extint_deasserted)return false;
  n.apic.lvt[3]=value;if(!fag_valid(&n))return false;
  *s=n;return true;
 }
 if(write && off==0x360 && value==0x400) {
  if(width!=4||!s->lint1_nmi_deasserted)return false;
  n.apic.lvt[4]=value;if(!fag_valid(&n))return false;
  *s=n;return true;
 }
 if(write && off==0xf0) {
  if(width!=4 || (value&~UINT32_C(0x1ff)) || (value&15)!=15)return false;
  n.apic.irq.svr=(uint16_t)value;
  if(!fag_valid(&n))return false;
  *s=n;return true;
 }
 if(write && off==0x320) {
  if(width!=4 || (value&~UINT32_C(0x300ff)))return false;
  n.apic.timer.lvt=value;
  if(!fag_valid(&n))return false;
  *s=n;return true;
 }
 if(!guest_apic_mmio(&n.apic,off,width,write,&value,0,&effect)||
 effect.eoi||effect.level||!fag_valid(&n))return false;
 *s=n;*v=value;return true;
}
