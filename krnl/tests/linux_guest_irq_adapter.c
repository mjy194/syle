#include "../linux_guest_irq_adapter.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct guest_apic a;static struct ioapic io;static struct gia c;
static void setup(bool on){guest_apic_init(&a,0);ioapic_init_guest(&io,gia_enqueue,&c);assert(gia_init(&c,&a,&io,99,on));io.pins[4].low=0x8028;struct guest_apic_effect e;assert(guest_apic_write(&a,0x80f,0x1ff,0,&e));}
int main(void){uint8_t v;uint64_t t;enum gia_source s;setup(false);assert(!gia_irq4(&c,4,true));
 setup(true);assert(gia_irq4(&c,4,true));assert(guest_apic_enqueue(&a,80,false));assert(gia_reserve(&c,&v,&t,&s)&&v==80&&s==GIA_LOCAL_EDGE);assert(!gia_irq4(&c,4,false));struct guest_apic before=a;assert(!gia_accept(&c,t+1,v)&&!memcmp(&a,&before,sizeof a));a.tmr[0]^=1;before=a;assert(!gia_accept(&c,t,v)&&!memcmp(&a,&before,sizeof a));a.tmr[0]^=1;assert(gia_accept(&c,t,v));assert(!(a.tmr[2]&(1u<<16)));
 struct guest_apic_effect e;assert(guest_apic_write(&a,0x80b,0,0,&e)&&e.eoi&&!e.level);assert(gia_reserve(&c,&v,&t,&s)&&v==40&&s==GIA_UART_LEVEL);assert(gia_accept(&c,t,v));assert(a.tmr[1]&(1u<<8));assert(!(a.pending_level[1]&(1u<<8)));
 assert(guest_apic_write(&a,0x80b,0,0,&e)&&e.level);a.irq.svr=0xff;assert(!gia_notify(&c,99,1,40));assert(c.receipt_pending&&!io.pins[4].remote_irr);assert(!gia_poll(&c,1));assert(!gia_notify(&c,99,2,40));a.irq.svr=0x1ff;assert(gia_notify(&c,99,1,40));assert(io.pins[4].remote_irr);struct ioapic saved=io;before=a;assert(gia_notify(&c,99,1,40));assert(!memcmp(&saved,&io,sizeof io)&&!memcmp(&before,&a,sizeof a));assert(!gia_notify(&c,100,1,40));assert(!gia_notify(&c,99,1,41));
 assert(gia_reserve(&c,&v,&t,&s));io.pins[4].low^=1;assert(!gia_accept(&c,t,v));io.pins[4].low^=1;assert(gia_cancel(&c,t,v));assert(gia_reserve(&c,&v,&t,&s));gia_quarantine(&c);assert(!gia_cancel(&c,t,v)&&!gia_accept(&c,t,v));
 setup(true);ioapic_init_guest(&io,gia_enqueue,&c);assert(gia_poll(&c,0));assert(guest_apic_enqueue(&a,80,false));assert(gia_reserve(&c,&v,&t,&s)&&v==80&&s==GIA_LOCAL_EDGE);assert(gia_accept(&c,t,v));
 puts("guest APIC multi-source adapter host tests PASS");}
