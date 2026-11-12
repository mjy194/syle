#include "linux_guest_dispatch.h"
#include "linux_guest_snapshot.h"
#include "linux_guest_manager.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct mock{uint64_t f[0x7000];unsigned reads,writes,invs,failread,failwrite;bool invfail,irqfail,reenter;};
static struct lgs_session session;
static struct lgd_context dispatch;
static struct mock mock;
static bool rd(void*p,uint32_t f,uint64_t*v){struct mock*m=p;assert(f<0x7000);if(++m->reads==m->failread)return false;*v=m->f[f];return true;}
static bool wr(void*p,uint32_t f,uint64_t v){struct mock*m=p;assert(f<0x7000);m->f[f]=v;return ++m->writes!=m->failwrite;}
static bool foreign_clock(void*p,uint64_t*t){(void)p;*t=0;return true;}
static bool inv(void*p,uint64_t e){struct mock*m=p;(void)e;m->invs++;return !m->invfail;}
static bool irq(void*p,unsigned n,bool level){struct mock*m=p;assert(n==4);(void)level;if(m->reenter){assert(!lgd_reset(&dispatch));assert(!lgd_dispatch(&dispatch,&session,52));assert(lgd_recv(&dispatch)==-16);}return !m->irqfail;}
static void setup(bool fixed){
 memset(&mock,0,sizeof mock);memset(&session,0,sizeof session);memset(&dispatch,0,sizeof dispatch);
 struct lgd_config cfg={.cpu={.basic_edx=UINT32_MAX,.extended_edx=UINT32_MAX,.physical_bits=32},.max_basic=1,.vendor_b=0x756e6547,.vendor_c=0x6c65746e,.vendor_d=0x49656e69,.max_extended=0x80000008,.max_exits=100,.fixed_input_verified=fixed,.irq_ctx=&mock,.sync_irq=irq};
 struct lgm_service_ops ops;assert(lgd_init(&dispatch,&cfg,&ops)&&ops.dispatch==lgd_dispatch&&ops.reset==lgd_reset);
 session.prepared=true;session.state.ready=true;session.state.guest_valid=true;
 session.platform.ctx=&mock;session.platform.read_field=rd;session.hop.context=&mock;session.hop.vmwrite=wr;session.hop.invept=inv;
 session.host.caps.cr0_fixed1=UINT64_MAX;session.host.caps.cr4_fixed1=UINT64_MAX;
 mock.f[0x681e]=0x10000;mock.f[0x0802]=0x10;mock.f[0x440c]=2;
 mock.f[0x6800]=mock.f[0x6004]=0x80010033;mock.f[0x6804]=0x2020;mock.f[0x6006]=0x20;mock.f[0x6802]=0x1000;mock.f[0x681c]=0x80000;
}
static bool run(unsigned reason){mock.f[0x4402]=reason;return lgd_dispatch(&dispatch,&session,reason);}
static void compressed(void){mock.f[0x681e]=0x19737f5;mock.f[0x6400]=0x1000000;mock.f[0x4404]=0x80000b0e;mock.f[0x4406]=2;mock.f[0x6818]=0x1979050;mock.f[0x4812]=0x1ff;mock.f[0x6802]=0x1991000;}
static uint8_t *ram;
static void*map(void*p,uint64_t pa){(void)p;return pa>=4096&&pa<=LGM_RAM_SIZE?ram+pa-4096:NULL;}
static void put(uint64_t a,uint64_t v){assert(a<=LGM_RAM_SIZE-8);for(unsigned i=0;i<8;i++)ram[a+i]=(uint8_t)(v>>(8*i));}
static void high(void){
 ram=calloc(1,LGM_RAM_SIZE);assert(ram);session.memory.prepared=true;session.memory.owner.map=map;
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)session.memory.pages[i]=(i+1ull)*4096;
 mock.f[0x4402]=0;mock.f[0x4404]=0x80000b0e;mock.f[0x4406]=0;mock.f[0x6400]=0xffff888000090000ull;
 mock.f[0x681e]=0xffffffff816cf50full;mock.f[0x6802]=0x16f5000;mock.f[0x6800]=0x80050033;mock.f[0x6804]=0x20b0;mock.f[0x2806]=0xd01;
 mock.f[0x681c]=0xffffffff81603f38ull;mock.f[0x0802]=0x10;mock.f[0x4816]=0xa09b;mock.f[0x0804]=0;mock.f[0x4818]=0x1c000;
 mock.f[0x6816]=0xffffffff81611000ull;mock.f[0x4810]=0x7f;mock.f[0x6818]=0xffffffff8174a000ull;mock.f[0x4812]=0xfff;
 session.native.gpr[4]=0xffff888000090000ull;session.native.gpr[5]=0xffffffff8174d760ull;session.native.gpr[2]=0x400;
 put(0x16f5000+511*8,0x7000003);put(0x7000000+510*8,0x7001003);
 put(0x7001000+9*8,0x1200083);put(0x7001000+11*8,0x1600083);
 put(0x1611010,0x00af9b000000ffffull);put(0x174a0e0,0x816c8e000010f3deull);put(0x174a0e8,0xffffffff);
 put(0x16cf508,0xf300000000000000ull);put(0x16cf510,0xa5);put(0x1736000,0);put(0x1634010,0);put(0x1634018,0x80000000000000e3ull);
 dispatch.pf=(struct lpfg_state){4,15,false,true};
}
int main(void){
 setup(false);assert(lgd_before_entry(&dispatch,&session)&&dispatch.entry_inflight);assert(!lgd_reset(&dispatch)&&lgd_send(&dispatch,1)==-16&&!run(52));
 struct lgs_entry_result receipt={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};
 assert(lgd_after_entry(&dispatch,&session,&receipt)&&!dispatch.entry_inflight);assert(run(52));assert(!lgd_after_entry(&dispatch,&session,&receipt));
 setup(false);assert(lgd_before_entry(&dispatch,&session));receipt=(struct lgs_entry_result){.state_result=LVS_GUEST_CAPTURE_FAILED};assert(!lgd_after_entry(&dispatch,&session,&receipt)&&dispatch.failed);
 setup(false);assert(lgd_before_entry(&dispatch,&session));receipt=(struct lgs_entry_result){.state_result=LVS_ENTRY_FAILED};assert(lgd_after_entry(&dispatch,&session,&receipt));
 setup(false);dispatch.exits=dispatch.config.max_exits;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed&&!mock.writes);
 setup(false);struct lgd_config enable=dispatch.config;struct lia adapter={.enabled=true};enable.irq_adapter=&adapter;enable.irq_delivery_enabled=true;enable.irq_platform_ready=true;
 struct lgd_context fresh={0};struct lgm_service_ops out={0};assert(!lgd_init(&fresh,&enable,&out)&&!fresh.configured&&!out.ctx);
 setup(false);struct guest_apic ga;struct ioapic gio;struct gia gdomain;struct lams_state gtx={0};struct lio_state giox={0};
 guest_apic_init_mode(&ga,0,false);ioapic_init(&gio,gia_enqueue,&gdomain);assert(gia_init(&gdomain,&ga,&gio,99,true));assert(lams_init(&gtx,&ga,99,&gdomain,gia_notify));assert(lio_init(&giox,&gdomain));
 enable=dispatch.config;enable.apic=(struct lad_config){.transactions=&gtx,.irq=&gdomain,.now=lgd_apic_now,.clock_ctx=&fresh,.ioapic=&giox};enable.irq_delivery_enabled=enable.irq_platform_ready=true;enable.irq_adapter=NULL;enable.sync_irq=gia_irq4;enable.irq_ctx=&gdomain;fresh=(struct lgd_context){0};out=(struct lgm_service_ops){0};enable.apic.now=foreign_clock;assert(!lgd_init(&fresh,&enable,&out));enable.apic.now=lgd_apic_now;
 assert(lgd_init(&fresh,&enable,&out)&&fresh.configured&&!fresh.activation.active);
 assert(lgd_send(&fresh,1)==-16&&lgd_recv(&fresh)==-16);
 assert(!lgd_before_entry(&fresh,&session)&&fresh.failed&&fresh.activation.failed&&!fresh.activation.active);
 assert(!lgd_before_entry(&fresh,&session));
 /* Quiescent session alone cannot discard a reserved/failed device domain. */
 dispatch.config.apic=enable.apic;gdomain.reserved=true;session.prepared=false;assert(!lgd_reset(&dispatch));
 setup(false);adapter.reserved=true;dispatch.config.irq_adapter=&adapter;assert(!lgd_reset(&dispatch));

 setup(false);session.native.gpr[0]=1;assert(run(10)&&mock.f[0x681e]==0x10002);assert(!(session.native.gpr[3]&(1u<<9)));assert(!lgd_reset(&dispatch));
 setup(false);uint64_t old=mock.f[0x681e];assert(run(52)&&mock.f[0x681e]==old);
 setup(false);dispatch.config.max_exits=1;assert(!run(52)&&dispatch.failed&&dispatch.exits==1&&!mock.reads&&!mock.writes);assert(!run(52)&&dispatch.exits==1);
 setup(false);dispatch.config.max_exits=2;assert(run(52)&&dispatch.exits==1);unsigned w=mock.writes,r=mock.reads;assert(!run(52)&&dispatch.failed&&dispatch.exits==2&&mock.writes==w&&mock.reads==r);assert(!run(52)&&dispatch.exits==2);
 setup(true);dispatch.config.max_exits=1;compressed();assert(!run(0)&&dispatch.exits==1&&!mock.writes&&!dispatch.pf.pending&&!dispatch.pf.count&&session.native.cr2==0);
 setup(true);dispatch.config.max_exits=2;assert(run(52));compressed();w=mock.writes;assert(!run(0)&&dispatch.exits==2&&mock.writes==w&&!dispatch.pf.pending&&!dispatch.pf.count&&session.native.cr2==0);
 setup(false);session.native.gpr[2]=0x8b;assert(run(32)&&!dispatch.microcode.loaded);session.native.gpr[0]=1;assert(run(10)&&dispatch.microcode.loaded);session.native.gpr[2]=0x8b;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;assert(run(31)&&!session.native.gpr[0]&&!session.native.gpr[3]);
 setup(false);session.native.gpr[2]=0x8b;session.native.gpr[0]=1;assert(!run(32));
 setup(false);session.native.gpr[2]=0x79;assert(!run(32));
 for(unsigned i=1;i<=4;i++){setup(false);session.native.gpr[0]=1;mock.failwrite=i;assert(!run(10)&&!dispatch.microcode.loaded);}
 setup(false);assert(!run(99)&&dispatch.failed);assert(!run(52));
 for(unsigned i=1;i<=4;i++){setup(false);session.native.gpr[0]=1;uint64_t g[15];memcpy(g,session.native.gpr,sizeof g);mock.failwrite=i;assert(!run(10)&&dispatch.failed&&!memcmp(g,session.native.gpr,sizeof g));}
 for(unsigned i=1;i<=7;i++){setup(false);mock.failread=i;assert(!run(10)&&dispatch.failed);}
 setup(false);mock.f[0x4408]=0x8000030e;assert(!run(52)&&!mock.writes);
 setup(false);mock.f[0x0802]=3;assert(!run(52));
 setup(false);mock.f[0x681e]=0x800000000000ull;assert(!run(52));
 setup(false);session.state.host_dirty=true;assert(!run(52)&&!mock.reads);
 setup(false);mock.f[0x440c]=3;mock.f[0x6400]=0x13;assert(run(28)&&session.native.gpr[0]==0x1000&&mock.f[0x681e]==0x10003);
 setup(false);mock.f[0x440c]=3;mock.f[0x6400]=0x3;session.native.gpr[0]=0x2000;mock.invfail=true;assert(!run(28)&&mock.f[0x681e]==0x10000);
 setup(false);session.native.gpr[2]=0xc0000100;session.native.gpr[0]=0x123000;assert(run(32)&&mock.f[0x680e]==0x123000);
 setup(false);session.native.gpr[2]=0xc0000102;session.native.gpr[0]=0x456000;assert(run(32)&&session.state.guest.msr[4]==0x456000);
 setup(false);session.native.gpr[2]=0xc0000080;mock.f[0x2806]=0x500;mock.f[0x4012]=0x200;session.native.gpr[0]=0xd01;assert(run(32)&&mock.f[0x2806]==0xd01&&mock.invs==1);
 setup(false);session.native.gpr[2]=0x480;assert(!run(31));
 setup(false);mock.f[0x440c]=1;mock.f[0x6400]=0x3fb0000;session.native.gpr[3]=0x3fb;session.native.gpr[0]=3;assert(run(30));
 mock.f[0x6400]=0x3f80000;session.native.gpr[3]=0x3f8;session.native.gpr[0]='Q';mock.irqfail=true;
 assert(run(30));mock.reenter=true;assert(lgd_recv(&dispatch)=='Q');assert(lgd_recv(&dispatch)==-16);assert(lgd_send(&dispatch,'R')==0);assert(lgd_send(&dispatch,'S')==-16);assert(dispatch.uart.irq_dirty);mock.irqfail=false;assert(run(52)&&!dispatch.uart.irq_dirty);
 setup(false);compressed();assert(!run(0)&&!dispatch.pf.pending);
 setup(true);compressed();assert(run(0)&&dispatch.pf.pending&&session.native.cr2==0x1000000&&mock.f[0x681e]==0x19737f5);
 mock.f[0x4016]=0xb0e;mock.f[0x681e]=0x19737f5;mock.f[0x6400]=0x1200000;assert(run(0)&&dispatch.pf.count==2);
 mock.f[0x4016]=0xb0e;mock.f[0x681e]=0x1974000;assert(run(52)&&dispatch.pf.closed&&!dispatch.pf.pending);
 for(unsigned i=1;i<=6;i++){setup(true);compressed();mock.failwrite=i;assert(!run(0)&&!dispatch.pf.pending&&dispatch.pf.count==0);}
 setup(true);dispatch.pf=(struct lpfg_state){1,1,true,false};mock.f[0x4016]=0xb0e;mock.failwrite=3;assert(!run(52)&&dispatch.pf.pending);
 for(unsigned i=1;i<=6;i++){setup(true);high();mock.failwrite=i;assert(!run(0)&&dispatch.high==LHP_UNUSED&&session.native.cr2==0);free(ram);}
 setup(true);dispatch.pf=(struct lpfg_state){4,15,false,true};dispatch.high=LHP_PENDING;mock.f[0x4016]=0xb0e;mock.failwrite=3;assert(!run(52)&&dispatch.high==LHP_PENDING);
 setup(true);high();struct lhp_snapshot snap;assert(lgds_high(&session,0,mock.f[0x681e],mock.f[0x6400],0,0,&snap)&&snap.permissions&&lhp_accept(LHP_UNUSED,&dispatch.pf,&snap));
 put(0x1634018,0xe3);assert(lgds_high(&session,0,mock.f[0x681e],mock.f[0x6400],0,0,&snap)&&!snap.permissions);put(0x1634018,0x80000000000000e3ull);
 assert(run(0)&&dispatch.high==LHP_PENDING&&session.native.cr2==0xffff888000090000ull);mock.f[0x4016]=0xb0e;
 assert(run(52)&&dispatch.high==LHP_CONSUMED);assert(!run(0));free(ram);
 session.prepared=false;session.enabled=false;session.memory.prepared=false;session.host.active=false;
 dispatch.microcode.loaded=true;
 assert(lgd_reset(&dispatch)&&!dispatch.microcode.loaded&&!dispatch.failed&&!dispatch.pf.count&&dispatch.high==LHP_UNUSED&&!dispatch.exits);
 puts("linux_guest_dispatch: CPU/CR/MSR/UART/PF/highPF, failures/budgets/reset PASS (host only)");
 return 0;
}
