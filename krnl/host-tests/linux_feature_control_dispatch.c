#define main original_tests
#include "linux_guest_dispatch.c"
#undef main
#include "linux_feature_control.h"
int main(void){original_tests();
 for(unsigned kind=0;kind<2;kind++){setup(false);uint64_t g[15];for(unsigned i=0;i<15;i++)g[i]=UINT64_MAX;g[2]=0x12340000003aull;uint64_t saved[15];memcpy(saved,g,sizeof g);
 assert(lfc_read(kind,&dispatch.config.cpu,g,0x1000,0,31,2,&mock,wr)&&g[0]==1&&!g[3]);for(unsigned i=0;i<15;i++)if(i!=0&&i!=3)assert(g[i]==saved[i]);
 struct lvp_cpuid_result cp;assert(lcp_query(kind,&dispatch.config.cpu,1,0,&cp)&&!(cp.c&96));assert(lcp_query(kind,&dispatch.config.cpu,7,0,&cp)&&!cp.b&&!cp.c);
 }
 for(unsigned k=0;k<8;k++){setup(false);uint64_t g[15]={0};g[2]=0x3a;unsigned reason=31,cpl=0,len=2,kind=0;uint64_t rip=0x1000;
 if(k==0)reason=32;
 if(k==1)cpl=3;
 if(k==2)len=1;
 if(k==3)g[2]=0x3b;
 if(k==4)kind=2;
 if(k==5)rip=0x800000000000ull;
 if(k==6)rip=UINT64_MAX;
 if(k==7)dispatch.config.cpu.basic_edx=0;
 assert(!lfc_read(kind,&dispatch.config.cpu,g,rip,cpl,reason,len,&mock,wr)&&!mock.writes);
 }
 setup(false);session.native.gpr[2]=0x3a;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;assert(run(31)&&session.native.gpr[0]==1&&!session.native.gpr[3]&&dispatch.ticks==1);
 setup(false);session.native.gpr[2]=0x3a;assert(!run(32)&&dispatch.failed&&!dispatch.ticks);
 setup(false);session.native.gpr[2]=0x3a;session.native.gpr[0]=session.native.gpr[3]=UINT64_MAX;mock.failwrite=4;assert(!run(31)&&dispatch.failed&&!dispatch.ticks&&session.native.gpr[0]==UINT64_MAX&&session.native.gpr[3]==UINT64_MAX);assert(!run(31));
 puts("FEATURE_CONTROL host PASS");}
