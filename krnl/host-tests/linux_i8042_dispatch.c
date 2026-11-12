#define main original_tests
#include "linux_i8042_fixture.c"
#undef main
#include "linux_i8042_absent.h"
static unsigned nw;static uint64_t newrip;static bool fail;
static bool writeone(void*c,uint32_t f,uint64_t v){(void)c;assert(f==0x681e);nw++;newrip=v;return !fail;}
static bool input(unsigned port,bool imm){session.native.gpr[0]=0x123456789abcdef0;session.native.gpr[3]=port;mock.f[0x6400]=((uint64_t)port<<16)|8|(imm?64:0);mock.f[0x440c]=imm?2:1;return run(30);}
int main(void){original_tests();
 for(unsigned port=0x60;port<=0x64;port+=4)for(unsigned imm=0;imm<2;imm++){
 uint64_t g[15],old[15];for(unsigned i=0;i<15;i++)g[i]=0x123456789abcde00+i;g[3]=port;memcpy(old,g,sizeof g);nw=0;fail=false;
 uint64_t q=((uint64_t)port<<16)|8|(imm?64:0);
 assert(li8042_read(g,0x1000,0,30,q,imm?2:1,NULL,writeone)&&nw==1&&newrip==0x1000+(imm?2u:1u));old[0]|=255;assert(!memcmp(g,old,sizeof g));
 for(unsigned bit=0;bit<64;bit++)if(bit!=6&&!(bit==18&&imm)){memcpy(g,old,sizeof g);nw=0;assert(!li8042_read(g,0x1000,0,30,q^(UINT64_C(1)<<bit),imm?2:1,NULL,writeone));assert(!nw&&!memcmp(g,old,sizeof g));}
 fail=true;nw=0;assert(!li8042_read(g,0x1000,0,30,q,imm?2:1,NULL,writeone)&&nw==1&&!memcmp(g,old,sizeof g));fail=false;
 for(unsigned k=0;k<7;k++){nw=0;uint64_t rip=k==2?UINT64_MAX:k==3?UINT64_C(0x7fffffffffff):k==4?UINT64_C(0x800000000000):0x1000;unsigned len=k==5?0:k==6?3:imm?2:1;assert(!li8042_read(g,rip,k==0, k==1?31:30,q,len,NULL,writeone)&&!nw);}
 }
 for(unsigned legacy=0;legacy<2;legacy++){
 setup(false);if(legacy){dispatch.config.legacy_pic_enabled=true;session.host.caps.primary=UINT64_C(0xffffffff00000000);mock.f[0x4002]=0x80000080;assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};assert(lgd_after_entry(&dispatch,&session,&r));}
 /* Linux v6.12 flush analogue: 17 status and 16 data reads, then EIO.
  * Delays are omitted here; this is a host behavior model, not guest boot. */
 unsigned data=0,status=0;int rc=0;
 while(1){assert(input(0x64,true));status++;if(!(session.native.gpr[0]&1))break;if(data<16){assert(input(0x60,false));data++;assert(session.native.gpr[0]==UINT64_C(0x123456789abcdeff));}else{rc=-5;break;}}
 assert(rc==-5&&data==16&&status==17&&dispatch.ticks==33);if(legacy)assert(dispatch.legacy.timer.timer.tick==33);
 }
 for(unsigned port=0x60;port<=0x64;port+=4)for(unsigned imm=0;imm<2;imm++){setup(false);assert(input(port,imm)&&session.native.gpr[0]==UINT64_C(0x123456789abcdeff)&&dispatch.ticks==1);}
 setup(false);mock.failwrite=4;uint64_t prior=0x123456789abcdef0;assert(!input(0x64,true)&&dispatch.failed&&!dispatch.ticks&&session.native.gpr[0]==prior);assert(!input(0x64,true));
 setup(false);mock.f[0x6400]=0x640040;mock.f[0x440c]=2;assert(!run(30)&&dispatch.failed&&!dispatch.ticks);
 puts("i8042 absence unit + real dispatcher PASS");
}
