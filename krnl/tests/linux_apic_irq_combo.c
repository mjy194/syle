#include "../linux_apic_msr.h"
#include "../linux_guest_irq_adapter.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct harness{struct guest_apic a;struct ioapic io;struct gia bridge;struct lams_state msr;unsigned mode,calls,writes;uint64_t rip;};
/* Fault injection exists ONLY in this host harness, never production paths:
 * 1=notification unavailable before delivery; 2=lost ack after successful sink;
 * 3=transient enqueue rejection via temporarily disabled APIC during sink. */
static bool notify(void*p,uint64_t d,uint64_t s,uint8_t v){struct harness*h=p;h->calls++;
 if(h->calls==1&&h->mode==1)return false;
 uint16_t saved=h->a.irq.svr;if(h->calls==1&&h->mode==3)h->a.irq.svr&=~0x100;
 bool ok=gia_notify(&h->bridge,d,s,v);
 if(h->calls==1&&h->mode==3)h->a.irq.svr=saved;
 return h->calls==1&&h->mode==2?false:ok;}
static bool write_rip(void*p,uint32_t f,uint64_t v){struct harness*h=p;assert(f==0x681e);h->writes++;h->rip=v;return true;}
static void setup(struct harness*h,unsigned mode){memset(h,0,sizeof *h);h->mode=mode;guest_apic_init(&h->a,0);ioapic_init_guest(&h->io,gia_enqueue,&h->bridge);assert(gia_init(&h->bridge,&h->a,&h->io,123,true));h->io.pins[4].low=0x8028;struct guest_apic_effect e;assert(guest_apic_write(&h->a,0x80f,0x1ff,0,&e));assert(lams_init(&h->msr,&h->a,123,h,notify));assert(gia_irq4(&h->bridge,4,true));uint8_t v;uint64_t t;enum gia_source src;assert(gia_reserve(&h->bridge,&v,&t,&src));assert(src==GIA_UART_LEVEL&&v==40);assert(gia_accept(&h->bridge,t,v));}
int main(void){for(unsigned mode=1;mode<=3;mode++){struct harness h;setup(&h,mode);uint64_t g[15]={0};g[2]=0x80b;
 assert(lams_apply(&h.msr,g,0x1000,0,32,2,0,1u<<21,1u<<9,gia_exclusive(&h.bridge),&h,write_rip)==LAMS_EFFECT_PENDING);
 assert(h.writes==1&&h.rip==0x1002&&h.msr.sequence==1&&h.msr.pending);assert(!(h.a.irq.isr[1]&(1u<<8)));
 if(mode==3)assert(h.bridge.receipt_pending&&!h.io.pins[4].remote_irr);
 uint64_t savedg[15];memcpy(savedg,g,sizeof g);assert(lams_apply(&h.msr,g,0x1000,0,32,2,0,1u<<21,1u<<9,true,&h,write_rip)==LAMS_REJECTED&&h.writes==1);
 assert(lams_retry(&h.msr)==LAMS_COMMITTED);assert(!h.msr.pending&&h.writes==1&&h.rip==0x1002&&!memcmp(savedg,g,sizeof g));assert(h.io.pins[4].remote_irr&&(h.a.irq.irr[1]&(1u<<8)));
 struct guest_apic before=h.a;struct ioapic io_before=h.io;unsigned calls=h.calls;
 assert(lams_retry(&h.msr)==LAMS_COMMITTED&&h.calls==calls);assert(gia_notify(&h.bridge,123,1,40));assert(!memcmp(&before,&h.a,sizeof before)&&!memcmp(&io_before,&h.io,sizeof io_before));
 assert(!gia_notify(&h.bridge,124,1,40));assert(!gia_notify(&h.bridge,123,1,41));assert(!gia_notify(&h.bridge,123,3,40));assert(!memcmp(&before,&h.a,sizeof before)&&!memcmp(&io_before,&h.io,sizeof io_before));
 /* Receive new level instance. Old receipt must not EOI this new ISR/remote. */
 uint8_t v;uint64_t t;enum gia_source src;assert(gia_reserve(&h.bridge,&v,&t,&src));assert(gia_accept(&h.bridge,t,v));before=h.a;io_before=h.io;assert(gia_notify(&h.bridge,123,1,40));assert(!memcmp(&before,&h.a,sizeof before)&&!memcmp(&io_before,&h.io,sizeof io_before));
 assert(lams_apply(&h.msr,g,0x1002,0,32,2,0,1u<<21,1u<<9,gia_exclusive(&h.bridge),&h,write_rip)==LAMS_COMMITTED);assert(h.writes==2&&h.rip==0x1004&&h.msr.sequence==2);assert(!gia_notify(&h.bridge,123,1,40));}
 puts("real LAMS + GIA + guest APIC + IOAPIC integration host tests PASS (3 failure modes)");}
