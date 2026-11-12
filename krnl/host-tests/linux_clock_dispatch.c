#define main dispatch_regression
#include "linux_guest_dispatch.c"
#undef main
static void pit_io(unsigned port,bool out,uint8_t value){
 session.native.gpr[0]=(session.native.gpr[0]&~255ull)|value;mock.f[0x440c]=2;
 mock.f[0x6400]=((uint64_t)port<<16)|64|(out?0:8);uint64_t old=dispatch.ticks;
 assert(run(30)&&dispatch.ticks==old+1&&!dispatch.clock_staged);
}
int main(void){setup(false);dispatch.config.max_exits=4096;uint64_t now;
 assert(lgd_apic_now(&dispatch,&now)&&!now);
 assert(lgd_before_entry(&dispatch,&session)&&!dispatch.ticks);
 struct lgs_entry_result receipt={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};
 assert(lgd_after_entry(&dispatch,&session,&receipt)&&!dispatch.ticks);
 assert(run(52)&&dispatch.ticks==1);session.native.gpr[0]=1;assert(run(10)&&dispatch.ticks==2);
 mock.f[0x440c]=1;mock.f[0x6400]=(0x3fdull<<16)|8;session.native.gpr[3]=0x3fd;
 assert(run(30)&&dispatch.ticks==3); /* existing DX UART unchanged */
 pit_io(0x61,false,0);assert((session.native.gpr[0]&255)==0);
 pit_io(0x61,true,1);pit_io(0x43,true,0xb0);pit_io(0x42,true,255);pit_io(0x42,true,255);
 pit_io(0x42,false,0);assert((session.native.gpr[0]&255)==255);
 pit_io(0x42,false,0);assert((session.native.gpr[0]&255)==255);
 unsigned samples=0;uint8_t last=255;
 while(last==255&&samples<200){pit_io(0x42,false,0);pit_io(0x42,false,0);last=(uint8_t)session.native.gpr[0];
 mock.f[0x440c]=2;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;
 uint64_t next=dispatch.ticks+1;assert(run(16)&&session.native.gpr[0]==(uint32_t)(1000*next)&&session.native.gpr[3]==0);samples++;}
 assert(last==254&&samples>5&&samples<200);assert(lgd_apic_now(&dispatch,&now)&&now==dispatch.ticks*1000);
 setup(true);compressed();assert(run(0)&&dispatch.ticks==1&&dispatch.pf.pending);
 setup(false);dispatch.ticks=UINT64_MAX/1000;unsigned reads=mock.reads,writes=mock.writes;
 assert(!run(52)&&dispatch.failed&&dispatch.ticks==UINT64_MAX/1000&&mock.reads==reads&&mock.writes==writes);
 setup(false);dispatch.clock_staged=true;assert(!run(52)&&dispatch.failed);
 setup(false);dispatch.pit.ticks=50;mock.f[0x440c]=2;mock.f[0x6400]=(0x61ull<<16)|72;
 assert(!run(30)&&dispatch.failed&&!dispatch.ticks); /* Backward device time stops. */
 setup(false);mock.failwrite=4;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;
 assert(!run(16)&&dispatch.failed&&!dispatch.ticks&&session.native.gpr[0]==UINT64_MAX&&session.native.gpr[3]==UINT64_MAX);
 setup(false);struct lei_irq event={0};struct lei_snapshot snapshot={.rflags=0x202};
 assert(lei_stage(&event,0x44,1,&snapshot,&mock,wr));lei_observe(&event,false,true,16,0);assert(event.phase==LEI_DELIVERED);
 event=(struct lei_irq){0};assert(lei_stage(&event,0x44,2,&snapshot,&mock,wr));lei_observe(&event,false,true,16,0x80000044);assert(event.phase==LEI_QUARANTINED);
 puts("linux_clock_dispatch: fixed PIT sequence/shared TSC, all-exit progression, no-entry advance, overflow/backward/partial failure PASS (host only, not calibration)");return 0;}
