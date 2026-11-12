#define main original_tests
#include "linux_idle_fixture.c"
#undef main
#include "linux_idle_pit.h"
static void writepic(struct pic0*p,unsigned port,uint8_t v){assert(pic0_access(p,port,true,&v));}
static void setup_timer(struct pt0*p,unsigned reload){assert(pt0_init(p,1));writepic(&p->pic,0x20,17);writepic(&p->pic,0x21,48);writepic(&p->pic,0x21,4);writepic(&p->pic,0x21,1);writepic(&p->pic,0x21,254);uint8_t v=0x34;assert(pit0_access(&p->timer,0,0x43,true,&v));v=reload;assert(pit0_access(&p->timer,0,0x40,true,&v));v=reload>>8;assert(pit0_access(&p->timer,0,0x40,true,&v));}
static void legacy(void){setup(false);dispatch.config.legacy_pic_enabled=dispatch.config.legacy_idle_advance=true;session.host.caps.primary=UINT64_C(0xffffffff00000000);mock.f[0x4002]=0x80000080;assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};assert(lgd_after_entry(&dispatch,&session,&r));setup_timer(&dispatch.legacy.timer,4773);mock.f[0x4826]=1;mock.f[0x6820]=512;}
int main(void){original_tests();struct pt0 p;setup_timer(&p,4773);struct lip_idle g={.reason=52,.activity=1,.flags=512};uint64_t next=0;assert(lip_plan(&p,0,&g,&next)&&next==4774);assert(pt0_tick(&p,next)&&p.pic.irr&&p.timer.edges==1);assert(lip_plan(&p,next,&g,&next)&&next==4775);
 setup_timer(&p,65536);assert(lip_plan(&p,0,&g,&next)&&next==1);assert(pt0_tick(&p,1));assert(lip_plan(&p,1,&g,&next)&&next==65537); /* start future bounded fallback */
 setup_timer(&p,10);assert(pt0_tick(&p,5));uint8_t b=4;assert(pit0_access(&p.timer,5,0x40,true,&b));b=0;assert(pit0_access(&p.timer,5,0x40,true,&b));assert(lip_plan(&p,5,&g,&next)&&next==11);assert(pt0_tick(&p,next)&&p.timer.reload==4&&p.timer.edges==1);
 for(unsigned f=0;f<4;f++){setup_timer(&p,10);struct lip_idle t=g;if(f==0)t.flags=0;if(f==1)t.activity=0;if(f==2)p.pic.imr=255;if(f==3)p.pic.isr=true;assert(lip_plan(&p,0,&t,&next)&&next==1);}
 for(unsigned f=0;f<6;f++){setup_timer(&p,10);struct lip_idle t=g;if(f==0)t.owner=true;if(f==1)t.event=1;if(f==2)t.error=1;if(f==3)t.length=1;if(f==4)t.vector=1;if(f==5)t.blocking=3;next=999;assert(!lip_plan(&p,0,&t,&next)&&next==999);}
 legacy();uint64_t rip=mock.f[0x681e];assert(run(52)&&dispatch.ticks==4774&&dispatch.legacy.timer.pic.irr&&dispatch.legacy.timer.timer.edges==1&&mock.f[0x4826]==1&&mock.f[0x681e]==rip&&dispatch.exits==1&&!dispatch.idle_staged);assert(lgd_before_entry(&dispatch,&session)&&dispatch.legacy.timer.pic.reserved&&mock.f[0x4826]==1); /* hardware must wake, not us */
 legacy();mock.f[0x6820]=0;assert(run(52)&&dispatch.ticks==1&&!dispatch.legacy.timer.pic.irr);
 legacy();mock.failwrite=mock.writes+1;assert(!run(52)&&dispatch.failed&&!dispatch.ticks&&!dispatch.legacy.timer.timer.edges&&!dispatch.legacy.timer.pic.irr);
 legacy();dispatch.bp.pending=true;unsigned w=mock.writes;assert(!run(52)&&dispatch.failed&&mock.writes==w&&!dispatch.ticks);
 legacy();dispatch.config.max_exits=1;assert(!run(52)&&dispatch.exits==1&&!dispatch.ticks);
 setup_timer(&p,10);p.timer.tick=UINT64_MAX/1000-1;p.timer.start=p.timer.tick;next=99;assert(!lip_plan(&p,p.timer.tick,&g,&next)&&next==99);
 setup_timer(&p,10);p.timer.start=UINT64_MAX-5;assert(!lip_plan(&p,0,&g,&next));
 for(unsigned readfail=1;readfail<=12;readfail++){legacy();mock.failread=mock.reads+readfail;assert(!run(52)&&dispatch.failed&&!dispatch.ticks&&!dispatch.legacy.timer.pic.irr);}
 legacy();assert(run(52));mock.f[0x4826]=0;mock.f[0x440c]=2;assert(run(16)&&session.native.gpr[0]==4775000&&dispatch.ticks==4775);mock.f[0x6400]=0x610048;assert(run(30)&&dispatch.pit.ticks==4776);mock.f[0x6400]=0x710048;assert(run(30)&&dispatch.ticks==4777);
 setup_timer(&p,10);p.pic.seen=1;assert(!lip_plan(&p,0,&g,&next));
 setup_timer(&p,10);assert(pt0_tick(&p,11));uint64_t token;uint8_t vector;assert(pic0_reserve(&p.pic,&token,&vector));assert(pic0_accept(&p.pic,1,token,vector));writepic(&p.pic,0x20,0x20);assert(lip_plan(&p,11,&g,&next)&&next==21); /* historical receipt is not pending */
 puts("idle PIT host PASS");}
