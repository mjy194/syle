#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
#define HZ UINT64_C(1193182)
static bool io(unsigned p,bool in){mock.f[0x6400]=((uint64_t)p<<16)|64|(in?8:0);mock.f[0x440c]=2;return run(30);}
static unsigned read_at(unsigned index,uint64_t tick){setup(false);dispatch.ticks=tick;session.native.gpr[0]=index;assert(io(0x70,false));session.native.gpr[0]=0x1234567800ULL;assert(io(0x71,true)&&session.native.gpr[0]>>8==0x12345678);return (uint8_t)session.native.gpr[0];}
int main(void){assert(read_at(0,0)==0&&read_at(2,0)==0&&read_at(4,0)==0&&read_at(6,0)==7&&read_at(7,0)==1&&read_at(8,0)==1&&read_at(9,0)==0);
 assert(read_at(0,HZ)==1&&read_at(2,60*HZ)==1&&read_at(4,3600*HZ)==1&&read_at(7,86400*HZ)==2);
 assert(read_at(7,59*86400*HZ)==0x29&&read_at(8,59*86400*HZ)==2&&read_at(7,60*86400*HZ)==1&&read_at(8,60*86400*HZ)==3);
 assert(read_at(9,366*86400*HZ)==1&&read_at(8,(366+59)*86400*HZ)==3);
 assert(read_at(10,HZ-294)==0xa6&&read_at(10,HZ-295)==0x26&&read_at(10,HZ)==0x26);
 assert(read_at(11,0)==2&&read_at(13,0)==0x80&&read_at(12,HZ)==0x50);
 setup(false);dispatch.ticks=HZ;session.native.gpr[0]=0x8c;assert(io(0x70,false)&&dispatch.rtc.nmi_mask&&dispatch.rtc.index==12);assert(io(0x71,true)&&session.native.gpr[0]==0x50);assert(io(0x71,true)&&session.native.gpr[0]==0);
 setup(false);session.native.gpr[0]=0x8a;mock.failwrite=4;assert(!io(0x70,false)&&!dispatch.rtc.nmi_mask&&!dispatch.rtc.index&&!dispatch.rtc.tick);
 setup(false);dispatch.ticks=HZ;session.native.gpr[0]=12;assert(io(0x70,false));mock.failwrite=mock.writes+4;session.native.gpr[0]=0xabcdef;assert(!io(0x71,true)&&dispatch.rtc.status_c==0x50&&session.native.gpr[0]==0xabcdef);
 setup(false);session.native.gpr[0]=11;assert(io(0x70,false));session.native.gpr[0]=0x42;assert(!io(0x71,false));
 setup(false);session.native.gpr[0]=1;assert(io(0x70,false));assert(!io(0x71,true));
 setup(false);dispatch.rtc.tick=100;assert(!io(0x70,false));
 setup(false);session.native.gpr[0]=0x8a;assert(io(0x70,false));session.prepared=false;assert(lgd_reset(&dispatch)&&!dispatch.rtc.tick&&!dispatch.rtc.nmi_mask&&!dispatch.rtc.index);
 assert(read_at(9,(UINT64_C(3155760000)-1)*HZ)==0x99);
 setup(false);dispatch.ticks=UINT64_C(3155760000)*HZ;assert(!io(0x70,false)); /* 2100 outside supported horizon, never falsely leap. */
 for(unsigned index=10;index<=11;index++){setup(false);session.native.gpr[0]=index;assert(io(0x70,false));session.native.gpr[0]=index==10?0x26:2;assert(io(0x71,false));session.native.gpr[0]=index==10?0x27:0x82;assert(!io(0x71,false));}
 setup(false);session.native.gpr[0]=0x8a;mock.f[0x6400]=0x700000;mock.f[0x440c]=1;session.native.gpr[3]=0x70;assert(run(30)&&dispatch.rtc.index==10&&dispatch.rtc.nmi_mask);
 const uint64_t bad[]={0x700041,0x700050,0x700060,0x100700040};for(unsigned i=0;i<4;i++){setup(false);mock.f[0x6400]=bad[i];assert(!run(30));}
 setup(false);assert(!lrtc_io(&dispatch.rtc,session.native.gpr,0x10000,1,30,0x700040,2,1,&mock,wr));
 setup(false);assert(!lrtc_io(&dispatch.rtc,session.native.gpr,0x7fffffffffffULL,0,30,0x700040,2,1,&mock,wr));
 setup(false);assert(!io(0x70,true));
 puts("linux_rtc: synthetic epoch/calendar/leap/UIP/flags, private NMI, read-clear failure, shared ticks PASS (host only)");return 0;}
