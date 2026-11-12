#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
int main(void){setup(false);session.native.gpr[0]=1;assert(run(10)&&!(session.native.gpr[3]&(1u<<9))&&!(session.native.gpr[2]&(1u<<21)));
 session.native.gpr[2]=0xfeed00000000001bULL;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;
 assert(run(31)&&session.native.gpr[0]==0xfee00100&&!session.native.gpr[3]&&dispatch.ticks==2);
 const uint64_t bad[]={0,0xfee00100,0xfee00900,0xfee00d00,0xfec00100,UINT64_MAX};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){setup(false);session.native.gpr[2]=0x1b;session.native.gpr[0]=(uint32_t)bad[i];session.native.gpr[3]=bad[i]>>32;
 assert(!run(32)&&dispatch.failed&&!dispatch.ticks);}
 setup(false);session.native.gpr[2]=0x1b;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;mock.failwrite=4;
 assert(!run(31)&&dispatch.failed&&!dispatch.ticks&&session.native.gpr[0]==UINT64_MAX&&session.native.gpr[3]==UINT64_MAX);
 puts("linux_disabled_apic_dispatch: hidden CPUID -> disabled APIC_BASE, every write rejected, clock/failure noresume PASS (host only)");return 0;}
