#define main pf_regression
#include "linux_nonpresent_pf.c"
#undef main
#include "../linux_guest_dispatch.h"
#include "linux_guest_manager.h"
static bool inv(void*x,uint64_t p){(void)x;(void)p;return true;}
static bool irq(void*x,unsigned n,bool v){(void)x;(void)v;return n==4;}
static void initialize(struct lgd_context*d){
 setup();s.hop.invept=inv;
 struct lgd_config c={.cpu={UINT32_MAX,UINT32_MAX,32},.max_basic=1,.vendor_b=0x756e6547,.vendor_c=0x6c65746e,.vendor_d=0x49656e69,.max_extended=0x80000008,.max_exits=100,.fixed_input_verified=true,.sync_irq=irq};
 struct lgm_service_ops out;*d=(struct lgd_context){0};assert(lgd_init(d,&c,&out));
 d->pf=(struct lpfg_state){.count=4,.seen=15,.closed=true};d->high=LHP_CONSUMED;
}
int main(void){struct lgd_context d;
 initialize(&d);assert(lgd_dispatch(&d,&s,0)&&d.demand_pf.pending&&d.demand_pf.count==1&&s.native.cr2==e.address);
 /* Arbitration-only fixture: bind a real pending legacy source directly;
  * this does not claim production APIC activation (tested separately). */
 struct lapic a;struct ioapic io;struct lia adapter;
 lapic_init(&a);assert(lapic_write(&a,0x1b,0xfee00d00)&&lapic_write(&a,0x80f,0x1ff));
 ioapic_init(&io,lia_enqueue,&adapter);assert(lia_init(&adapter,&a,&io,true));
 io.pins[4].low=0x8044;io.pins[4].remote_irr=true;assert(lapic_enqueue_external(&a,0x44));
 uint8_t selected;assert(lia_peek(&adapter,&selected)&&selected==0x44);
 d.config.irq_delivery_enabled=true;d.config.irq_adapter=&adapter;fields[0x6820]=0x202;
 unsigned prior=writes;assert(lgd_before_entry(&d,&s)&&!adapter.reserved&&writes==prior&&fields[0x4016]==0x80000b0e);
 struct lgs_entry_result receipt={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};assert(lgd_after_entry(&d,&s,&receipt));
 unsigned n=writes;fields[0x4402]=52;assert(!lgd_dispatch(&d,&s,52)&&d.failed&&d.demand_pf.pending&&writes==n);
 initialize(&d);assert(lgd_dispatch(&d,&s,0));fields[0x4402]=10;fields[0x440c]=2;s.native.gpr[0]=1;
 assert(lgd_dispatch(&d,&s,10)&&!d.demand_pf.pending&&d.demand_pf.count==1);
 fields[0x4402]=0;fields[0x681e]=e.rip;assert(!lgd_dispatch(&d,&s,0)&&d.failed&&d.demand_pf.count==1);
 initialize(&d);d.high=LHP_UNUSED;assert(!lgd_dispatch(&d,&s,0)&&!d.demand_pf.count);
 initialize(&d);d.demand_pf.count=LNP_MAX_FAULTS;assert(!lgd_dispatch(&d,&s,0)&&d.demand_pf.count==LNP_MAX_FAULTS);
 puts("linux_nonpresent_dispatch: prior phase fence, actual injection, instruction receipt, timer isolation and repeat/budget stop PASS (host only)");return 0;}
