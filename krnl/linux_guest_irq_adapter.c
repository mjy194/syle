#include "linux_guest_irq_adapter.h"
static void snapshot(unsigned char*d,const void*p,unsigned n){const unsigned char*s=p;for(unsigned i=0;i<n;i++)d[i]=s[i];}
static bool same(const unsigned char*a,const void*p,unsigned n){const unsigned char*b=p;for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
bool gia_exclusive(const struct gia*c){return c&&c->enabled&&c->apic&&c->io&&!c->reserved&&!c->quarantined&&!c->receipt_pending&&!c->delivering;}
static bool route(const struct gia*c){if(!c||!c->io||c->io->enqueue!=gia_enqueue||c->io->context!=c)return false;
 uint32_t low=c->io->pins[4].low;if(!(low&(1u<<15))||(low&~(0xffu|(1u<<13)|(1u<<15)|(1u<<16)))||(low&255)<32)return false;
 for(unsigned i=0;i<IOAPIC_PINS;i++){if(i!=4&&!(c->io->pins[i].low&(1u<<16)))return false;}
 return true;}
bool gia_init(struct gia*c,struct guest_apic*a,struct ioapic*i,uint64_t domain,bool on){if(!c||!a||!i||!domain)return false;*c=(struct gia){.apic=a,.io=i,.domain=domain,.enabled=on};return true;}
bool gia_peek(const struct gia*c,uint8_t*v,enum gia_source*s){uint8_t n;if(!v||!s||!gia_exclusive(c)||!guest_apic_select(c->apic,&n))return false;
 bool level=(c->apic->pending_level[n/32]&(1u<<(n%32)))!=0;
 if(level&&(!route(c)||n!=(uint8_t)c->io->pins[4].low||!c->io->pins[4].remote_irr))return false;
 /* Edge may be timer or self-IPI; shared IRR does not retain finer provenance. */
 *v=n;*s=level?GIA_UART_LEVEL:GIA_LOCAL_EDGE;return true;}
bool gia_reserve(struct gia*c,uint8_t*v,uint64_t*t,enum gia_source*s){uint8_t n;enum gia_source src;if(!t||!v||!s||!gia_peek(c,&n,&src)||c->generation==UINT64_MAX)return false;
 snapshot(c->saved,c->apic,sizeof *c->apic);snapshot(c->saved_io,c->io,sizeof *c->io);c->token=++c->generation;c->vector=n;c->source=src;c->reserved=true;*v=n;*t=c->token;*s=src;return true;}
static bool owned(struct gia*c,uint64_t t,uint8_t v){return c&&c->enabled&&c->reserved&&!c->quarantined&&c->token==t&&c->vector==v&&same(c->saved,c->apic,sizeof *c->apic)&&same(c->saved_io,c->io,sizeof *c->io);}
bool gia_accept(void*p,uint64_t t,uint8_t v){struct gia*c=p;if(!owned(c,t,v)||!guest_apic_accept(c->apic,v))return false;c->reserved=false;c->token=0;return true;}
bool gia_cancel(struct gia*c,uint64_t t,uint8_t v){if(!owned(c,t,v))return false;c->reserved=false;c->token=0;return true;}
void gia_quarantine(struct gia*c){if(c&&c->reserved)c->quarantined=true;}
bool gia_enqueue(void*p,uint8_t v){struct gia*c=p;if(!c||!c->enabled||c->reserved||c->quarantined||(c->receipt_pending&&!c->delivering)||!route(c)||v!=(uint8_t)c->io->pins[4].low)return false;return guest_apic_enqueue(c->apic,v,true);}
bool gia_irq4(void*p,unsigned pin,bool level){struct gia*c=p;if(pin!=4||!gia_exclusive(c)||!route(c))return false;return ioapic_set_irq(c->io,4,level);}
bool gia_poll(struct gia*c,uint64_t now){return gia_exclusive(c)&&guest_apic_poll(c->apic,now);}
enum device_status gia_ioapic_mmio(struct gia*c,uint64_t off,unsigned width,bool write,uint32_t*v){if(!gia_exclusive(c))return DEVICE_FAULT;return ioapic_mmio(c->io,off,width,write,v);}
bool gia_notify(void*p,uint64_t domain,uint64_t seq,uint8_t v){struct gia*c=p;
 if(!c||!c->enabled||!c->apic||!c->io||c->reserved||c->quarantined||c->delivering||domain!=c->domain||!seq||!route(c))return false;
 if(seq==c->receipt_sequence){if(v!=c->receipt_vector)return false;if(!c->receipt_pending)return true;
  c->delivering=true;bool ok=ioapic_retry(c->io,4);c->delivering=false;if(ok)c->receipt_pending=false;return ok;}
 if(c->receipt_pending||c->receipt_sequence==UINT64_MAX||seq!=c->receipt_sequence+1||v!=(uint8_t)c->io->pins[4].low||!c->io->pins[4].remote_irr)return false;
 /* LAMS already popped ISR; record durable identity BEFORE modifying IOAPIC.
  * Retry must never clear a newly delivered remote IRR a second time. */
 c->receipt_sequence=seq;c->receipt_vector=v;c->receipt_pending=true;c->delivering=true;
 bool ok=ioapic_eoi(c->io,v);c->delivering=false;if(ok)c->receipt_pending=false;return ok;
}
