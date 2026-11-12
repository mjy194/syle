#define main gate_regression
#include "linux_profile_gate.c"
#undef main
#include "../linux_guest_dispatch.h"
#include "linux_guest_manager.h"
static bool write_live(void*c,uint32_t field,uint64_t value){(void)c;f[field]=value;return true;}
static bool inv_live(void*c,uint64_t value){(void)c;(void)value;return true;}
int main(void){
 gate_regression();ram=calloc(1,LGM_RAM_SIZE);tables=calloc(LGM_TABLE_COUNT,4096);assert(ram&&tables);setup();
 s.hop.vmwrite=s.host.owner.vmwrite=write_live;s.hop.invept=inv_live;
 struct lgd_config conf={.cpu=host,.apic=cfg,.irq_delivery_enabled=true,.irq_platform_ready=true,
 .max_basic=1,.vendor_b=0x756e6547,.vendor_c=0x6c65746e,.vendor_d=0x49656e69,.max_extended=0x80000008,
 .max_exits=100,.irq_ctx=&gia,.sync_irq=gia_irq4};
 struct lgd_context d={0};struct lgm_service_ops ops;
 struct lgd_context ungated={0};assert(lgd_init(&ungated,&conf,&ops));
 s.native.gpr[0]=1;f[0x4402]=10;f[0x440c]=2;
 assert(!lgd_dispatch(&ungated,&s,10)&&ungated.failed&&!ungated.activation.active);
 /* Reset the fixture GPR/VMCS evidence before first-entry verification. */
 s.native.gpr[0]=0;f[0x4402]=0;f[0x440c]=0;
 assert(lgd_init(&d,&conf,&ops)&&!d.activation.active&&d.activation.kind==LCP_BASELINE);
 assert(lgd_send(&d,1)==-16&&lgd_before_entry(&d,&s)&&d.activation.active);
 struct lgs_entry_result result={.state_result=LVS_EXIT,.fields_valid=true,.reason=10};
 assert(lgd_after_entry(&d,&s,&result));f[0x4402]=10;f[0x440c]=2;s.native.gpr[0]=1;
 assert(lgd_dispatch(&d,&s,10)&&(s.native.gpr[3]&(1u<<9))&&(s.native.gpr[2]&(1u<<21))&&d.microcode.loaded);
 /* Same profile powers internal APIC MSR checks, not only guest CPUID. */
 f[0x4402]=31;s.native.gpr[2]=0x1b;assert(lgd_dispatch(&d,&s,31)&&s.native.gpr[0]==0xfee00900);
 ram[LAT_GPA]^=1;s.exits=2;s.native.resume=1;current_time=2;
 assert(lgd_before_entry(&d,&s));result.reason=52;assert(lgd_after_entry(&d,&s,&result));
 f[0x4002]|=1u<<28;assert(!lgd_before_entry(&d,&s)&&d.failed&&d.activation.failed);
 assert(!lgd_dispatch(&d,&s,10)&&lgd_send(&d,1)==-16&&!lgd_reset(&d));
 s.prepared=s.enabled=s.host.active=s.memory.prepared=false;assert(lgd_reset(&d)&&!d.activation.active&&d.activation.kind==LCP_BASELINE);
 free(ram);free(tables);puts("linux_profile_activation: actual gate -> shared CPUID/MSR view, sticky control failure, teardown reset PASS (host only)");
}
