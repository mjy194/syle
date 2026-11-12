#include "../linux_irq_glue.h"
#include <assert.h>
#include <stdio.h>
static unsigned writes;
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;(void)f;(void)v;writes++;return true;}
int main(void){
 struct lapic a;struct ioapic io;struct lia adapter;struct lig_state g={0};
 lapic_init(&a);assert(lapic_write(&a,0x1b,0xfee00d00)&&lapic_write(&a,0x80f,0x1ff));
 ioapic_init(&io,lia_enqueue,&adapter);assert(lia_init(&adapter,&a,&io,true));
 /* Test fixture config only: production glue never writes guest routing. */
 uint32_t value=0x18;assert(lia_ioapic_mmio(&adapter,0,4,true,&value)==DEVICE_HANDLED);
 value=0x8044;assert(lia_ioapic_mmio(&adapter,0x10,4,true,&value)==DEVICE_HANDLED);
 assert(lia_irq4(&adapter,4,true)&&io.pins[4].remote_irr);
 struct lei_snapshot s={.rflags=0x202};
 assert(lig_before(&g,&adapter,&s,0,wr)&&adapter.reserved&&g.event.phase==LEI_STAGED);
 value=0;assert(lia_ioapic_mmio(&adapter,0,4,true,&value)!=DEVICE_HANDLED);
 assert(!lia_irq4(&adapter,4,false));
 assert(lig_after(&g,&adapter,LIG_EXIT,10,0,0,wr)&&!adapter.reserved);
 assert(!(a.irr[2]&(1u<<4))&&(a.isr[2]&(1u<<4))&&writes==6);
 assert(lia_irq4(&adapter,4,false));bool retired=false;uint8_t vector=0;
 assert(lia_eoi(&adapter,&retired,&vector)&&retired&&vector==0x44&&!io.pins[4].remote_irr);
 assert(lia_irq4(&adapter,4,true));assert(lig_before(&g,&adapter,&s,0,wr));
 assert(lig_after(&g,&adapter,LIG_NO_ENTRY,0,0,0,wr)&&!adapter.reserved&&(a.irr[2]&(1u<<4)));
 assert(lig_before(&g,&adapter,&s,0,wr));assert(!lig_after(&g,&adapter,LIG_EXIT,52,0,0,wr)&&adapter.quarantined&&adapter.reserved);
 puts("linux_irq_glue_devices: real LAPIC/IOAPIC reservation lifecycle PASS (host only)");
}
