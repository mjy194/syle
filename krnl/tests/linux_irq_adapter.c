#include "../linux_irq_adapter.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct lapic a;static struct ioapic io;static struct lia c;
static void setup(bool on){lapic_init(&a);ioapic_init_guest(&io,lia_enqueue,&c);assert(lia_init(&c,&a,&io,on));io.pins[4].low=0x8028;assert(lapic_write(&a,0x1b,0xfee00d00));assert(lapic_write(&a,0x80f,0x1ff));}
int main(void){uint8_t v=9;uint64_t t=9;setup(false);assert(!lia_irq4(&c,4,true));assert(!lia_reserve(&c,&v,&t));
 setup(true);io.pins[4].low=40;assert(!lia_irq4(&c,4,true));io.pins[4].low=0x8028;
 assert(lia_irq4(&c,4,true));assert(io.pins[4].remote_irr);assert(lapic_enqueue_external(&a,80));uint8_t oldv=v;assert(!lia_peek(&c,&v)&&v==oldv);a.irr[2]=0;assert(lia_reserve(&c,&v,&t)&&v==40&&t==1);uint32_t sel=0x18;assert(lia_ioapic_mmio(&c,0,4,true,&sel)==DEVICE_FAULT);
 struct lapic before=a;assert(!lia_irq4(&c,4,false));assert(!lia_enqueue(&c,40));assert(!lia_accept(&c,t+1,v));assert(!memcmp(&a,&before,sizeof a));
 assert(lia_cancel(&c,t,v));assert(!memcmp(&a,&before,sizeof a));assert(lia_reserve(&c,&v,&t)&&t==2);
 a.tpr=0x30;before=a;assert(!lia_accept(&c,t,v));assert(!memcmp(&a,&before,sizeof a)&&c.reserved);a.tpr=0;
 io.pins[4].low^=(1u<<16);before=a;assert(!lia_accept(&c,t,v));assert(!memcmp(&a,&before,sizeof a)&&c.reserved);io.pins[4].low^=(1u<<16);
 assert(lia_accept(&c,t,v));assert(!lia_accept(&c,t,v));assert(a.isr[1]&(1u<<8));bool retired=false;uint8_t popped=0;
 assert(lia_eoi(&c,&retired,&popped)&&retired&&popped==40);assert(a.irr[1]&(1u<<8));
 assert(lia_reserve(&c,&v,&t));lia_quarantine(&c);assert(!lia_cancel(&c,t,v)&&!lia_accept(&c,t,v));
 setup(true);a.svr=0xff;assert(!lia_irq4(&c,4,true));assert(io.pins[4].asserted&&!io.pins[4].remote_irr);a.svr=0x1ff;assert(lia_irq4(&c,4,true));
 setup(true);assert(lia_irq4(&c,4,true));assert(lia_reserve(&c,&v,&t));assert(lia_accept(&c,t,v));a.svr=0xff;retired=false;assert(!lia_eoi(&c,&retired,&popped)&&retired&&popped==40);assert(!a.isr[1]);a.svr=0x1ff;assert(lia_irq4(&c,4,true));assert(a.irr[1]&(1u<<8));
 puts("IRQ LAPIC adapter host tests PASS");}
