#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
#include "../linux_sysenter_msr.h"
int main(void){
 const uint32_t ids[]={0x174,0x175,0x176},fields[]={0x482a,0x6824,0x6826};
 const uint64_t values[]={0x10,0xffff888000001000ULL,0xffffffff81001234ULL};
 for(unsigned i=0;i<3;i++){
  setup(false);mock.f[0x4c00]=0x30;mock.f[0x6c10]=0x1000;mock.f[0x6c12]=0x2000;session.native.gpr[2]=0xfeed000000000000ULL|ids[i];session.native.gpr[0]=values[i];session.native.gpr[3]=values[i]>>32;
  assert(run(32)&&mock.f[fields[i]]==values[i]&&dispatch.ticks==1);
  session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;
  assert(run(31)&&session.native.gpr[0]==(uint32_t)values[i]&&session.native.gpr[3]==(uint32_t)(values[i]>>32));
  assert(mock.f[0x4c00]==0x30&&mock.f[0x6c10]==0x1000&&mock.f[0x6c12]==0x2000);
  setup(false);session.native.gpr[2]=ids[i];mock.f[fields[i]]=i?0x800000000000ULL:65536;assert(!run(31)&&mock.writes==3);
  for(unsigned fail=4;fail<=5;fail++){setup(false);session.native.gpr[2]=ids[i];session.native.gpr[0]=values[i];session.native.gpr[3]=values[i]>>32;
   uint64_t g[15];memcpy(g,session.native.gpr,sizeof g);mock.failwrite=fail;
   assert(!run(32)&&dispatch.failed&&!dispatch.ticks&&!memcmp(g,session.native.gpr,sizeof g));unsigned writes=mock.writes;assert(!run(32)&&mock.writes==writes);}
  setup(false);session.native.gpr[2]=ids[i];session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;mock.f[fields[i]]=values[i];mock.failwrite=4;
  assert(!run(31)&&session.native.gpr[0]==UINT64_MAX&&session.native.gpr[3]==UINT64_MAX);
  setup(false);session.native.gpr[2]=ids[i];session.native.gpr[0]=i?0:65536;session.native.gpr[3]=i?0x8000:0;assert(!run(32)&&dispatch.failed&&mock.f[fields[i]]==0);
 }
 const uint64_t bounds[]={0,0x7fffffffffffULL,0xffff800000000000ULL,UINT64_MAX};
 for(unsigned i=0;i<4;i++){setup(false);session.native.gpr[2]=0x176;session.native.gpr[0]=bounds[i];session.native.gpr[3]=bounds[i]>>32;assert(run(32)&&mock.f[0x6826]==bounds[i]);}
 setup(false);session.native.gpr[2]=0x174;session.native.gpr[0]=65535;assert(run(32)&&mock.f[0x482a]==65535);
 setup(false);session.native.gpr[2]=0x174;mock.f[0x681e]=0x7fffffffffffULL;assert(!run(32));
 setup(false);session.native.gpr[0]=1;assert(run(10)&&!(session.native.gpr[3]&(1u<<11)));
 setup(false);session.native.gpr[2]=0x174;mock.failread=1;
 assert(!lse_apply(session.native.gpr,0x10000,0,32,2,1u<<5,1u<<29,&mock,rd,&mock,wr)&&!mock.writes);
 setup(false);session.native.gpr[2]=0x174;
 assert(!lse_apply(session.native.gpr,0x10000,1,32,2,1u<<5,1u<<29,&mock,rd,&mock,wr));
 assert(!lse_apply(session.native.gpr,0x10000,0,32,1,1u<<5,1u<<29,&mock,rd,&mock,wr));
 assert(!lse_apply(session.native.gpr,0x10000,0,32,2,0,1u<<29,&mock,rd,&mock,wr));
 assert(!lse_apply(session.native.gpr,0x10000,0,32,2,1u<<5,0,&mock,rd,&mock,wr));
 session.native.gpr[2]=0x177;assert(!lse_apply(session.native.gpr,0x10000,0,32,2,1u<<5,1u<<29,&mock,rd,&mock,wr));
 assert(!mock.writes);
 puts("linux_sysenter_dispatch: guest-only CS/ESP/EIP roundtrip, partial failure noresume, SEP unchanged PASS (host only)");return 0;
}
