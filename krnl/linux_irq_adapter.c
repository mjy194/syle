#include "linux_irq_adapter.h"
static bool available(const struct lia *c){return c&&c->enabled&&c->apic&&c->io&&!c->reserved&&!c->quarantined;}
static bool unchanged(const struct lia*c){const struct lapic*a=c->apic,*b=&c->saved;
 if(a->base!=b->base||a->tpr!=b->tpr||a->svr!=b->svr)return false;
 const struct ioapic*x=c->io,*y=&c->saved_io;
 if(x->selector!=y->selector||x->guest_profile!=y->guest_profile||x->enqueue!=y->enqueue||x->context!=y->context)return false;
 for(unsigned i=0;i<IOAPIC_PINS;i++)if(x->pins[i].low!=y->pins[i].low||x->pins[i].asserted!=y->pins[i].asserted||x->pins[i].remote_irr!=y->pins[i].remote_irr)return false;
 for(unsigned i=0;i<8;i++)if(a->irr[i]!=b->irr[i]||a->isr[i]!=b->isr[i])return false;
 return true;
}
static bool route(const struct lia*c){
 /* Device source accepts only physical dest0/fixed; verify stored low fields.
  * Other pins must remain masked in this deliberately single-source profile. */
 if(!c||!c->io||c->io->enqueue!=lia_enqueue||c->io->context!=c)return false;
 uint32_t low=c->io->pins[4].low;
 if(!(low&(1u<<15))||(low&~(0xffu|(1u<<13)|(1u<<15)|(1u<<16)))||(low&255)<32)return false;
 for(unsigned i=0;i<IOAPIC_PINS;i++)if(i!=4&&!(c->io->pins[i].low&(1u<<16)))return false;
 return true;
}
bool lia_init(struct lia*c,struct lapic*a,struct ioapic*i,bool on){if(!c||!a||!i)return false;*c=(struct lia){.apic=a,.io=i,.enabled=on};return true;}
bool lia_peek(const struct lia*c,uint8_t*v){uint8_t n;if(!v||!available(c)||!route(c)||!c->io->pins[4].remote_irr||!lapic_select(c->apic,&n)||n!=(uint8_t)c->io->pins[4].low)return false;*v=n;return true;}
bool lia_reserve(struct lia*c,uint8_t*v,uint64_t*t){uint8_t n;if(!v||!t||!lia_peek(c,&n)||c->generation==UINT64_MAX)return false;
 c->saved=*c->apic;c->saved_io=*c->io;c->vector=n;c->token=++c->generation;c->reserved=true;*v=n;*t=c->token;return true;}
bool lia_accept(void*p,uint64_t t,uint8_t v){struct lia*c=p;
 if(!c||!c->enabled||!c->reserved||c->quarantined||t!=c->token||v!=c->vector||!unchanged(c))return false;
 if(!lapic_accept(c->apic,v))return false;
 c->reserved=false;c->token=0;return true;}
bool lia_cancel(struct lia*c,uint64_t t,uint8_t v){if(!c||!c->reserved||c->quarantined||t!=c->token||v!=c->vector||!unchanged(c))return false;c->reserved=false;c->token=0;return true;}
void lia_quarantine(struct lia*c){if(c&&c->reserved)c->quarantined=true;}
bool lia_enqueue(void*p,uint8_t v){struct lia*c=p;return available(c)&&route(c)&&v==(uint8_t)c->io->pins[4].low&&lapic_enqueue_external(c->apic,v);}
bool lia_irq4(void*p,unsigned pin,bool asserted){struct lia*c=p;if(pin!=4||!available(c)||!route(c))return false;return ioapic_set_irq(c->io,4,asserted);}
bool lia_eoi(struct lia*c,bool*retired,uint8_t*v){if(!retired||!v||!available(c)||!route(c)||c->apic->base!=0xfee00d00)return false;
 uint8_t n;if(!lapic_eoi_vector(c->apic,&n)){*retired=false;return true;}
 *retired=true;*v=n;return ioapic_eoi(c->io,n);}

enum device_status lia_ioapic_mmio(struct lia*c,uint64_t o,unsigned w,bool write,uint32_t*v){if(!available(c))return DEVICE_FAULT;return ioapic_mmio(c->io,o,w,write,v);}
