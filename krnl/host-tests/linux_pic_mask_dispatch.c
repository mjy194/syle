#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
static bool io(unsigned port,bool input,bool immediate){mock.f[0x6400]=((uint64_t)port<<16)|(input?8:0)|(immediate?64:0);mock.f[0x440c]=immediate?2:1;session.native.gpr[3]=port;return run(30);}
int main(void){setup(false);session.native.gpr[0]=255;assert(io(0xa1,false,true));session.native.gpr[0]=251;assert(io(0x21,false,true));session.native.gpr[0]=0x123456789abc0000ULL;assert(io(0x21,true,true)&&session.native.gpr[0]==0x123456789abc00fbULL);assert(dispatch.ticks==3);
 assert(io(0xa1,true,false)&&(uint8_t)session.native.gpr[0]==255);session.native.gpr[0]=255;assert(io(0x21,false,false)&&dispatch.pic.master==255);
 const unsigned ports[]={0x21,0xa1};for(unsigned p=0;p<2;p++)for(unsigned v=0;v<256;v++){setup(false);session.native.gpr[0]=v;bool safe=v==255||(!p&&v==251);assert(io(ports[p],false,true)==safe);if(!safe)assert(dispatch.failed&&dispatch.pic.master==255&&dispatch.pic.slave==255);}
 setup(false);session.native.gpr[0]=251;mock.failwrite=4;assert(!io(0x21,false,true)&&dispatch.pic.master==255&&dispatch.failed);unsigned n=mock.writes;assert(!io(0x21,false,true)&&mock.writes==n);
 setup(false);session.native.gpr[0]=0x123456789;mock.failwrite=4;assert(!io(0x21,true,true)&&session.native.gpr[0]==0x123456789);
 setup(false);session.native.gpr[0]=0x11;assert(!io(0x20,false,true));setup(false);assert(!io(0xa0,true,true));
 const uint64_t bad[]={0x210041,0x210050,0x210060,0x100210040};for(unsigned i=0;i<4;i++){setup(false);mock.f[0x6400]=bad[i];assert(!run(30));}
 setup(false);mock.f[0x6400]=0x210000;mock.f[0x440c]=1;session.native.gpr[3]=0x22;assert(!run(30));
 setup(false);session.native.gpr[0]=251;assert(io(0x21,false,true));session.prepared=false;assert(lgd_reset(&dispatch)&&dispatch.pic.master==255&&dispatch.pic.slave==255);
 setup(false);assert(!lpic_io(&dispatch.pic,session.native.gpr,0x10000,1,30,0x210040,2,&mock,wr)&&dispatch.pic.failed);
 const uint64_t rips[]={0x800000000000ULL,0x7fffffffffffULL,UINT64_MAX};for(unsigned i=0;i<3;i++){setup(false);assert(!lpic_io(&dispatch.pic,session.native.gpr,rips[i],0,30,0x210048,2,&mock,wr)&&!mock.writes);}
 setup(false);assert(!lpic_io(&dispatch.pic,session.native.gpr,0x10000,0,30,0x210048,1,&mock,wr));
 setup(false);session.native.gpr[3]=0x21;assert(!lpic_io(&dispatch.pic,session.native.gpr,0x10000,0,30,0x210008,2,&mock,wr));
 setup(false);dispatch.pic.slave=254;assert(!io(0x21,true,true)&&dispatch.pic.failed);
 setup(false);assert(lgd_reset(&dispatch)&&lgd_reset(&dispatch)&&dispatch.pic.master==255&&dispatch.pic.slave==255);
 puts("linux_pic_mask: real FF/FB cascade masks, exhaustive unmask rejection, scalar IO/failures/reset PASS (host only, no PIC delivery)");return 0;}
