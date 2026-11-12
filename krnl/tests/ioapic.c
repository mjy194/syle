#include "../ioapic.h"
#include <assert.h>
#include <stdio.h>
struct sink {unsigned count;uint8_t last;bool fail;};
static bool enqueue(void *ctx,uint8_t vector) {
    struct sink *s=ctx;
    if(s->fail) return false;
    s->count++;s->last=vector;return true;
}
static enum device_status reg(struct ioapic *a,unsigned r,bool write,uint32_t *v) {
    uint32_t sel=r;
    assert(ioapic_mmio(a,0,4,true,&sel)==DEVICE_HANDLED);
    return ioapic_mmio(a,0x10,4,write,v);
}
static uint32_t readreg(struct ioapic *a,unsigned r) {
    uint32_t v=0xdeadbeef;assert(reg(a,r,false,&v)==DEVICE_HANDLED);return v;
}
static enum device_status writereg(struct ioapic *a,unsigned r,uint32_t v) {
    return reg(a,r,true,&v);
}
static enum device_status dummy(void *ctx,uint64_t off,unsigned width,bool write,uint32_t *v) {
    (void)ctx;(void)off;(void)width;(void)write;*v=0xabc;return DEVICE_HANDLED;
}
int main(void) {
    struct device_bus bus;device_bus_init(&bus);
    assert(!device_bus_register(&bus,0,0,dummy,0));
    assert(!device_bus_register(&bus,1,4,dummy,0));
    assert(!device_bus_register(&bus,0,5,dummy,0));
    assert(!device_bus_register(&bus,UINT64_MAX-3,4,dummy,0));
    assert(!device_bus_register(&bus,0,4,0,0));
    assert(device_bus_register(&bus,0x1000,0x1000,dummy,0));
    assert(!device_bus_register(&bus,0x1800,0x1000,dummy,0));
    assert(!device_bus_register(&bus,0,0x2000,dummy,0));
    assert(!device_bus_register(&bus,0x1000,4,dummy,0));
    assert(device_bus_register(&bus,0x2000,4,dummy,0));
    uint32_t v=0;
    assert(device_bus_access(&bus,0x1ffc,4,false,&v)==DEVICE_HANDLED && v==0xabc);
    assert(device_bus_access(&bus,0x1ffe,4,false,&v)==DEVICE_FAULT);
    assert(device_bus_access(&bus,0x1ffc,8,false,&v)==DEVICE_FAULT);
    assert(device_bus_access(&bus,UINT64_MAX-3,4,false,&v)==DEVICE_FAULT);
    assert(device_bus_access(&bus,0x1000,4,false,0)==DEVICE_FAULT);
    assert(device_bus_access(&bus,0x2004,4,false,&v)==DEVICE_UNSUPPORTED);
    for(unsigned i=2;i<DEVICE_BUS_REGIONS;i++)
        assert(device_bus_register(&bus,0x3000+i*4,4,dummy,0));
    assert(!device_bus_register(&bus,0x4000,4,dummy,0));

    struct sink sink={0};struct ioapic a;ioapic_init(&a,enqueue,&sink);
    device_bus_init(&bus);assert(device_bus_register(&bus,0xfec00000,4096,ioapic_mmio,&a));
    v=1;assert(device_bus_access(&bus,0xfec00000,4,true,&v)==DEVICE_HANDLED);
    assert(device_bus_access(&bus,0xfec00010,4,false,&v)==DEVICE_HANDLED && v==0x00170011);
    assert(readreg(&a,0)==0 && readreg(&a,2)==0);
    assert(writereg(&a,0,0)==DEVICE_UNSUPPORTED);
    assert(writereg(&a,1,0)==DEVICE_UNSUPPORTED);
    assert(writereg(&a,2,0)==DEVICE_UNSUPPORTED);
    for(unsigned i=0;i<IOAPIC_PINS;i++) {
        assert(readreg(&a,0x10+2*i)==0x10000);
        assert(readreg(&a,0x11+2*i)==0);
    }
    v=256;assert(ioapic_mmio(&a,0,4,true,&v)==DEVICE_FAULT);
    assert(ioapic_mmio(&a,1,4,true,&v)==DEVICE_FAULT);
    assert(ioapic_mmio(&a,0,8,true,&v)==DEVICE_FAULT);
    assert(ioapic_mmio(&a,4,4,false,&v)==DEVICE_UNSUPPORTED);
    assert(writereg(&a,0x40,0)==DEVICE_UNSUPPORTED);
    assert(writereg(&a,0x11,1u<<24)==DEVICE_FAULT);
    for(unsigned bit=8;bit<32;bit++) if(bit!=15 && bit!=16)
        assert(writereg(&a,0x10,0x40|(1u<<bit))==DEVICE_FAULT);
    assert(writereg(&a,0x10,31)==DEVICE_FAULT);
    assert(!ioapic_set_irq(&a,24,true));assert(!ioapic_retry(&a,24));

    /* Masked rising edges are discarded, not replayed on unmask. */
    assert(writereg(&a,0x10,0x10040)==DEVICE_HANDLED);
    assert(ioapic_set_irq(&a,0,true));assert(sink.count==0);
    assert(writereg(&a,0x10,0x40)==DEVICE_HANDLED);assert(sink.count==0);
    assert(ioapic_set_irq(&a,0,true));assert(sink.count==0);
    assert(ioapic_set_irq(&a,0,false));assert(ioapic_set_irq(&a,0,true));
    assert(sink.count==1 && sink.last==0x40 && !(readreg(&a,0x10)&0x4000));
    assert(ioapic_set_irq(&a,0,true));assert(sink.count==1);
    assert(writereg(&a,0x12,0x40)==DEVICE_FAULT); /* duplicate unmasked vector */
    assert(writereg(&a,0x12,0x10040)==DEVICE_HANDLED);
    assert(writereg(&a,0x12,0x40)==DEVICE_FAULT);

    /* Asserted masked level delivers immediately when unmasked. */
    assert(writereg(&a,0x12,0x18050)==DEVICE_HANDLED);
    assert(ioapic_set_irq(&a,1,true));assert(sink.count==1);
    assert(writereg(&a,0x12,0x8050)==DEVICE_HANDLED);
    assert(sink.count==2 && sink.last==0x50 && (readreg(&a,0x12)&0x4000));
    assert(writereg(&a,0x12,0x18050)==DEVICE_FAULT); /* all route changes blocked */
    assert(writereg(&a,0x13,0)==DEVICE_FAULT);
    assert(ioapic_set_irq(&a,1,true));assert(sink.count==2);
    assert(ioapic_eoi(&a,0x51));assert(sink.count==2);
    assert(ioapic_eoi(&a,0x50));assert(sink.count==3 && (readreg(&a,0x12)&0x4000));
    assert(ioapic_set_irq(&a,1,false));assert(readreg(&a,0x12)&0x4000);
    assert(ioapic_eoi(&a,0x50));assert(sink.count==3 && !(readreg(&a,0x12)&0x4000));

    /* Failed enqueue retains asserted level, never marks remote IRR. */
    sink.fail=true;
    assert(!ioapic_set_irq(&a,1,true));assert(!(readreg(&a,0x12)&0x4000));
    sink.fail=false;assert(ioapic_retry(&a,1));assert(sink.count==4);
    sink.fail=true;assert(!ioapic_eoi(&a,0x50));assert(!(readreg(&a,0x12)&0x4000));
    sink.fail=false;assert(ioapic_retry(&a,1));assert(sink.count==5);
    assert(ioapic_set_irq(&a,1,false));assert(ioapic_eoi(&a,0x50));
    /* Failed unmask delivery commits valid route; retry is explicit. */
    assert(writereg(&a,0x14,0x18060)==DEVICE_HANDLED);
    assert(ioapic_set_irq(&a,2,true));sink.fail=true;
    assert(writereg(&a,0x14,0x8060)==DEVICE_FAULT);
    assert(readreg(&a,0x14)==0x8060);
    sink.fail=false;assert(ioapic_retry(&a,2));assert(sink.count==6);
    /* Edge enqueue failures are not replayed. */
    assert(ioapic_set_irq(&a,0,false));sink.fail=true;
    assert(!ioapic_set_irq(&a,0,true));sink.fail=false;
    assert(ioapic_retry(&a,0));assert(ioapic_set_irq(&a,0,true));assert(sink.count==6);
    assert(ioapic_set_irq(&a,0,false));assert(ioapic_set_irq(&a,0,true));assert(sink.count==7);
    puts("PASS device bus and limited 24-pin IOAPIC route/line/EOI policy");
}
