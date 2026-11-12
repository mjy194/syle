#include "../linux_guest_irq_glue.h"
#include "../linux_apic_msr.h"
#include <assert.h>
#include <stdio.h>
static struct guest_apic a;static struct ioapic io;static struct gia adapter;static struct lig_state glue;static struct lei_snapshot snapshot;static unsigned writes;
static bool wr(void*c,uint32_t f,uint64_t v){(void)c;(void)f;(void)v;writes++;return true;}
static void setup(void){guest_apic_init(&a,0);ioapic_init(&io,gia_enqueue,&adapter);assert(gia_init(&adapter,&a,&io,74,true));glue=(struct lig_state){0};snapshot=(struct lei_snapshot){.rflags=0x202};writes=0;struct guest_apic_effect e;assert(guest_apic_write(&a,0x80f,0x1ff,0,&e));}
int main(void){
 setup();struct guest_apic_effect e;assert(guest_apic_write(&a,0x832,0x45,0,&e)&&guest_apic_write(&a,0x838,1,0,&e));assert(gia_poll(&adapter,a.timer.deadline));
 assert(lgig_before(&glue,&adapter,&snapshot,0,wr)&&adapter.source==GIA_LOCAL_EDGE&&adapter.reserved);assert(lgig_after(&glue,&adapter,LIG_EXIT,10,0,0,wr)&&!adapter.reserved&&(a.irq.isr[2]&(1u<<5))&&!(a.tmr[2]&(1u<<5)));
 setup();assert(guest_apic_enqueue(&a,0x45,false));snapshot.pf_pending=true;assert(lgig_before(&glue,&adapter,&snapshot,0,wr)&&!adapter.reserved&&!writes);snapshot.pf_pending=false;snapshot.high_pending=true;assert(lgig_before(&glue,&adapter,&snapshot,0,wr)&&!adapter.reserved);snapshot.high_pending=false;snapshot.rflags=2;assert(lgig_before(&glue,&adapter,&snapshot,0,wr)&&!adapter.reserved);snapshot.rflags=0x202;snapshot.blocking=1;assert(lgig_before(&glue,&adapter,&snapshot,0,wr)&&!adapter.reserved);
 setup();uint32_t v=0x18;assert(gia_ioapic_mmio(&adapter,0,4,true,&v)==DEVICE_HANDLED);v=0x8044;assert(gia_ioapic_mmio(&adapter,0x10,4,true,&v)==DEVICE_HANDLED);assert(gia_irq4(&adapter,4,true));assert(lgig_before(&glue,&adapter,&snapshot,0,wr)&&adapter.source==GIA_UART_LEVEL);assert(lgig_after(&glue,&adapter,LIG_EXIT,32,0,0,wr)&&(a.tmr[2]&(1u<<4)));
 struct lams_state tx={0};assert(lams_init(&tx,&a,74,&adapter,gia_notify));uint64_t g[15]={0};g[2]=0x80b;assert(lams_apply(&tx,g,0x1000,0,32,2,0,1u<<21,1u<<9,gia_exclusive(&adapter),0,wr)==LAMS_COMMITTED);assert(io.pins[4].remote_irr&&(a.irq.irr[2]&(1u<<4))&&!(a.irq.isr[2]&(1u<<4)));
 assert(gia_notify(&adapter,74,1,0x44)&&io.pins[4].remote_irr);assert(lgig_before(&glue,&adapter,&snapshot,0,wr));assert(!lgig_after(&glue,&adapter,LIG_EXIT,52,0,0,wr)&&adapter.quarantined&&adapter.reserved&&glue.failed);
 assert(!lgig_before(&glue,&adapter,&snapshot,0,wr));
 puts("linux_guest_irq_glue: actual local timer/UART level/EOI/PF/defer/quarantine PASS (host only)");
}
