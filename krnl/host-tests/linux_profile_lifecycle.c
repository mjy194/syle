#define main gate_regression
#include "linux_profile_gate.c"
#undef main
#include "../linux_profile_prepare.h"
#include "../linux_guest_dispatch.h"
#include "linux_guest_manager.h"
static unsigned writebacks,vmwrites,release_calls;static bool fail_vmwrite;
static bool write_live(void*c,uint32_t field,uint64_t v){(void)c;vmwrites++;if(fail_vmwrite&&field==0x681e)return false;f[field]=v;return true;}
static bool wb(void*c,uint64_t pa){(void)c;(void)pa;writebacks++;return true;}
static bool release(void*c,uint64_t pa){(void)c;(void)pa;release_calls++;return true;}
static bool inv(void*c,uint64_t v){(void)c;(void)v;return true;}
static void entered_exit(struct lgd_context*d,uint32_t reason){
 assert(lgd_before_entry(d,&s));
 struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=reason};
 assert(lgd_after_entry(d,&s,&r));s.exits++;s.native.resume=1;f[0x4402]=reason;
}
int main(void){
 ram=calloc(1,LGM_RAM_SIZE);tables=calloc(LGM_TABLE_COUNT,4096);assert(ram&&tables);setup();
 /* Fixture supplies only allocated RAM/EPT and VMCS values. All global device
  * objects/config are invalidated so they cannot stand in for the wrapper. */
 memset(&cfg,0,sizeof cfg);memset(&gia,0,sizeof gia);memset(&apic,0,sizeof apic);
 memset(&io,0,sizeof io);memset(&tx,0,sizeof tx);memset(&lio,0,sizeof lio);
 memset(ram+LAT_GPA,0,LAR_SIZE);put(ram+0x90070,0,8);
 s.prepared=s.enabled=s.host.ready=s.host.active=false;
 s.memory.owner.writeback=wb;s.memory.owner.release=release;
 struct lpp_context *p=calloc(1,sizeof *p);assert(p&&lpp_init(p,987,NULL,now));
 struct lgd_config config={.cpu=host,.apic=p->config,.irq_delivery_enabled=true,.irq_platform_ready=true,
 .max_basic=1,.vendor_b=0x756e6547,.vendor_c=0x6c65746e,.vendor_d=0x49656e69,.max_extended=0x80000008,
 .max_exits=100,.irq_ctx=&p->irq,.sync_irq=gia_irq4};
 struct lgd_context d={0};struct lgm_service_ops ops;assert(lgd_init(&d,&config,&ops)&&lgd_reset(&d));
 assert(lgd_send(&d,1)==-16&&lpp_prepare_guest(p,&s)&&writebacks==2&&!release_calls&&!vmwrites);
 assert(p->firmware==LAI_INSTALLED&&p->session==&s&&p->owner.map==s.memory.owner.map);
 assert(p->irq.apic==&p->apic&&p->irq.io==&p->io&&p->transactions.model==&p->apic&&p->config.irq==&p->irq);
 /* Mock the host's VMCS construction/enable, never execute VMX. First gate reads
  * precisely the RAM installed above, through the same owned-page callbacks. */
 s.prepared=s.enabled=s.host.ready=s.host.active=true;
 s.hop.vmwrite=s.host.owner.vmwrite=write_live;s.hop.invept=inv;
 entered_exit(&d,10);assert(d.activation.active&&d.activation.config.irq==&p->irq&&d.activation.domain==987);
 f[0x440c]=2;s.native.gpr[0]=1;assert(lgd_dispatch(&d,&s,10)&&(s.native.gpr[3]&(1u<<9))&&(s.native.gpr[2]&(1u<<21)));
 entered_exit(&d,31);s.native.gpr[2]=0x1b;assert(lgd_dispatch(&d,&s,31)&&s.native.gpr[0]==0xfee00900);
 /* Real MOV decoder + owned guest four-level walk + xAPIC version transaction. */
 put(ram+0x1000,0x2023,8);put(ram+0x2000,0x3023,8);put(ram+0x3000,0x4023,8);
 put(ram+0x4000,0x8063,8);put(ram+0x4008,0xfee00063,8);ram[0x8000]=0x8b;ram[0x8001]=3;
 f[0x681e]=0;f[0x0802]=0x10;f[0x4816]=0xa09b;f[0x6800]=0x80010033;f[0x6802]=0x1000;f[0x6804]=0x2020;f[0x2806]=0xd01;
 f[0x6400]=0x181;f[0x640a]=0x1030;f[0x2400]=0xfee00030;s.native.gpr[1]=0x1030;
 entered_exit(&d,48);assert(lgd_dispatch(&d,&s,48)&&s.native.gpr[0]==0x60014&&f[0x681e]==2);
 /* A failed next transaction cannot release RAM or restart the domain. */
 entered_exit(&d,10);s.native.gpr[0]=1;uint64_t old=s.native.gpr[0];d.microcode.loaded=false;fail_vmwrite=true;
 assert(!lgd_dispatch(&d,&s,10)&&d.failed&&!d.microcode.loaded&&s.native.gpr[0]==old&&!release_calls&&s.memory.prepared);
 assert(!lgd_reset(&d)&&!lpp_prepare_guest(p,&s));
 /* Explicit owner teardown is represented here, not performed by wrappers. */
 s.prepared=s.enabled=s.host.ready=s.host.active=s.memory.prepared=false;assert(lgd_reset(&d));
 assert(p->prepared&&p->firmware==LAI_INSTALLED&&!lpp_prepare_guest(p,&s)&&!release_calls);
 free(p);free(ram);free(tables);puts("linux_profile_lifecycle: same owned domain prepare -> gate -> CPUID/MSR/xAPIC MOV -> sticky failure/owner cleanup PASS (host only)");return 0;
}
