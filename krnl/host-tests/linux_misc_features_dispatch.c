#define main original_tests
#include "linux_guest_dispatch.c"
#undef main
#include "linux_misc_features.h"
int main(void){original_tests();for(unsigned k=0;k<2;k++){setup(false);uint64_t g[15];for(unsigned i=0;i<15;i++)g[i]=UINT64_MAX;g[2]=0xab0000000140ull;assert(lmf_apply(k,&dispatch.config.cpu,g,0x1000,0,31,2,&mock,wr)&&!g[0]&&!g[3]);g[0]=g[3]=0xffffffff00000000ull;assert(lmf_apply(k,&dispatch.config.cpu,g,0x1000,0,32,2,&mock,wr)&&g[0]==0xffffffff00000000ull&&g[3]==g[0]);}
 for(unsigned b=0;b<64;b++){setup(false);uint64_t g[15]={0};g[2]=0x140;g[b<32?0:3]=1ull<<(b%32);assert(!lmf_apply(0,&dispatch.config.cpu,g,0x1000,0,32,2,&mock,wr)&&!mock.writes);}
 setup(false);session.native.gpr[2]=0x140;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;assert(run(31)&&!session.native.gpr[0]&&!session.native.gpr[3]&&dispatch.ticks==1);assert(run(32)&&dispatch.ticks==2);
 setup(false);session.native.gpr[2]=0x140;session.native.gpr[0]=1;assert(!run(32)&&dispatch.failed&&!dispatch.ticks);
 setup(false);session.native.gpr[2]=0x140;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;mock.failwrite=4;assert(!run(31)&&dispatch.failed&&session.native.gpr[0]==UINT64_MAX&&session.native.gpr[3]==UINT64_MAX);assert(!run(31));
 for(unsigned bad=0;bad<6;bad++){setup(false);uint64_t g[15]={0};g[2]=0x140;unsigned len=2,cpl=0,kind=0;uint64_t rip=0x1000;if(bad==0){len=1;}if(bad==1){cpl=3;}if(bad==2){kind=9;}if(bad==3){rip=UINT64_MAX;}if(bad==4){g[2]=0x141;}if(bad==5){dispatch.config.cpu.basic_edx=0;}assert(!lmf_apply(kind,&dispatch.config.cpu,g,rip,cpl,31,len,&mock,wr)&&!mock.writes);}
 puts("MISC_FEATURES host PASS");}
