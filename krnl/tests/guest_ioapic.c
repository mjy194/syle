#include "../ioapic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct sink { unsigned count; uint8_t vector; };
static bool enqueue(void *ctx,uint8_t vector) {
    struct sink *s=ctx;s->count++;s->vector=vector;return true;
}
static enum device_status wr(struct ioapic *a,uint32_t v) {
    uint32_t sel=0x10;
    assert(ioapic_mmio(a,0,4,true,&sel)==DEVICE_HANDLED);
    return ioapic_mmio(a,0x10,4,true,&v);
}
int main(void) {
    struct ioapic a;struct sink s={0};
    ioapic_init_guest(&a,enqueue,&s);
    assert(wr(&a,0x1a040)==DEVICE_HANDLED); /* masked low-active level */
    assert(ioapic_set_level(&a,0,true)); /* high is deasserted */
    assert(!a.pins[0].asserted && s.count==0);
    assert(wr(&a,0xa040)==DEVICE_HANDLED);
    assert(ioapic_set_level(&a,0,false));
    assert(s.count==1 && s.vector==0x40 && a.pins[0].remote_irr);
    assert(wr(&a,0x1e040)==DEVICE_HANDLED); /* RMW includes RO remote-IRR */
    assert(a.pins[0].remote_irr && a.pins[0].low==0x1a040);
    /* While pin0 is masked in service, another pin can claim its vector.
     * Unmask must enforce uniqueness before any state mutation. */
    uint32_t sel=0x12,value=0x40;
    assert(ioapic_mmio(&a,0,4,true,&sel)==DEVICE_HANDLED);
    assert(ioapic_mmio(&a,0x10,4,true,&value)==DEVICE_HANDLED);
    sel=0x10;
    assert(ioapic_mmio(&a,0,4,true,&sel)==DEVICE_HANDLED);
    struct ioapic before;memcpy(&before,&a,sizeof a);
    value=0xe040;
    assert(ioapic_mmio(&a,0x10,4,true,&value)==DEVICE_FAULT);
    assert(memcmp(&before,&a,sizeof a)==0 && value==0xe040 && s.count==1);
    sel=0x12;value=0x10040;
    assert(ioapic_mmio(&a,0,4,true,&sel)==DEVICE_HANDLED);
    assert(ioapic_mmio(&a,0x10,4,true,&value)==DEVICE_HANDLED);
    assert(wr(&a,0xe040)==DEVICE_HANDLED && a.pins[0].remote_irr);
    assert(s.count==1); /* unmask while Remote IRR set cannot redeliver */
    assert(wr(&a,0x1e040)==DEVICE_HANDLED);
    assert(wr(&a,0x1a041)==DEVICE_FAULT); /* cannot retarget inflight */
    assert(wr(&a,0x12040)==DEVICE_FAULT); /* cannot change trigger */
    assert(ioapic_eoi(&a,0x40));
    assert(!a.pins[0].remote_irr && s.count==1); /* masked, no redelivery */
    assert(wr(&a,0xa040)==DEVICE_HANDLED);
    assert(s.count==2 && a.pins[0].remote_irr); /* asserted level replays */
    assert(ioapic_set_level(&a,0,true));
    assert(ioapic_eoi(&a,0x40) && s.count==2);
    assert(wr(&a,0x2041)==DEVICE_HANDLED); /* low-active edge */
    assert(ioapic_set_level(&a,0,true));
    assert(ioapic_set_level(&a,0,false));
    assert(s.count==3 && s.vector==0x41);
    assert(ioapic_set_level(&a,0,false) && s.count==3);
    assert(ioapic_set_level(&a,0,true));
    assert(ioapic_set_level(&a,0,false) && s.count==4);
    assert(!ioapic_set_level(&a,IOAPIC_PINS,false));
    assert(!ioapic_set_level(NULL,0,false));
    ioapic_init(&a,enqueue,&s);
    assert(wr(&a,0xa040)==DEVICE_FAULT); /* old policy unchanged */
    puts("PASS guest IOAPIC polarity, in-service mask/EOI and edge delivery");
}
