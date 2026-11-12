#include "../lapic.h"
#include "../ioapic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool queue(void *ctx,uint8_t v) {return lapic_enqueue_external(ctx,v);}
int main(void) {
    struct lapic a,old;uint8_t v=123;
    lapic_init(&a);
    memcpy(&old,&a,sizeof a);
    assert(!lapic_enqueue_external(&a,64)); /* xAPIC not supported */
    assert(!lapic_eoi_vector(&a,&v) && v==123);
    assert(memcmp(&a,&old,sizeof a)==0);
    assert(lapic_write(&a,0x1b,0xfee00d00));
    assert(!lapic_enqueue_external(&a,64)); /* SVR disabled */
    assert(lapic_write(&a,0x80f,0x1ff));
    assert(!lapic_enqueue_external(&a,31));
    assert(!lapic_enqueue_external(&a,256));
    assert(!lapic_enqueue_external(NULL,64));
    assert(lapic_write(&a,0x808,0xf0));
    assert(lapic_enqueue_external(&a,64));
    assert(lapic_enqueue_external(&a,64));
    assert(!lapic_select(&a,&v)); /* queue despite TPR, coalesce */
    assert(a.irr[2]==1);
    assert(lapic_write(&a,0x808,0));
    assert(lapic_select(&a,&v) && v==64);
    assert(a.irr[2]==1 && a.isr[2]==0);
    assert(lapic_accept(&a,v));
    assert(lapic_enqueue_external(&a,80));
    assert(lapic_accept(&a,80));
    assert(lapic_enqueue_external(&a,64)); /* same vector pending during ISR */
    assert(!lapic_select(&a,&v));
    memcpy(&old,&a,sizeof a);
    assert(!lapic_eoi_vector(&a,NULL) && memcmp(&a,&old,sizeof a)==0);
    assert(lapic_eoi_vector(&a,&v) && v==80);
    assert(lapic_eoi_vector(&a,&v) && v==64);
    assert(lapic_select(&a,&v) && v==64);
    assert(lapic_accept(&a,64));
    assert(lapic_write(&a,0x80b,0));
    v=123;assert(!lapic_eoi_vector(&a,&v) && v==123);
    assert(lapic_write(&a,0x80b,0)); /* empty EOI succeeds */
    struct ioapic io;ioapic_init_guest(&io,queue,&a);
    uint32_t sel=0x18,route=0x8040;
    assert(ioapic_mmio(&io,0,4,true,&sel)==DEVICE_HANDLED);
    assert(ioapic_mmio(&io,0x10,4,true,&route)==DEVICE_HANDLED);
    assert(ioapic_set_irq(&io,4,true) && io.pins[4].remote_irr);
    assert(lapic_accept(&a,64));
    assert(lapic_eoi_vector(&a,&v) && v==64);
    assert(ioapic_eoi(&io,v)); /* asserted line queues next */
    assert(lapic_select(&a,&v) && v==64);
    assert(ioapic_set_irq(&io,4,false));
    assert(lapic_accept(&a,64));
    assert(lapic_eoi_vector(&a,&v) && ioapic_eoi(&io,v));
    assert(!lapic_select(&a,&v) && !io.pins[4].remote_irr);
    puts("PASS external LAPIC enqueue, priority/coalescing and vector EOI/IOAPIC bridge");
}
