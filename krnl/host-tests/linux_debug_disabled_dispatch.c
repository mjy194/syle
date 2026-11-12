#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
static void init(void){setup(false);mock.f[0x4002]=1u<<23;mock.f[0x681a]=0x400;mock.f[0x4816]=0xa09b;mock.f[0x2806]=0x500;mock.f[0x440c]=3;}
int main(void){
 const unsigned map[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};
 for(unsigned r=0;r<16;r++){init();uint64_t val=0xffff888000000000ULL+4096*r;
 if(r==4)mock.f[0x681c]=val;else session.native.gpr[map[r]]=val;mock.f[0x6400]=r<<8;
 assert(run(29)&&dispatch.debug_disabled.address[0]==val);
 mock.f[0x6400]=(r<<8)|16;assert(run(29));assert((r==4?mock.f[0x681c]:session.native.gpr[map[r]])==val);}
 const unsigned ds[]={0,1,2,3,6,7};for(unsigned i=0;i<6;i++){init();mock.f[0x6400]=ds[i];assert(run(29));mock.f[0x6400]=ds[i]|16;assert(run(29));assert(session.native.gpr[0]==(ds[i]<4?0:ds[i]==6?0xffff0ff0u:0x400));}
 const uint64_t bad[]={1,0x2000,0x401,UINT64_MAX};for(unsigned i=0;i<4;i++){init();mock.f[0x6400]=7;session.native.gpr[0]=bad[i];assert(!run(29)&&dispatch.failed&&mock.f[0x681a]==0x400);}
 for(unsigned f=4;f<=5;f++){init();dispatch.debug_disabled.address[0]=0x70000;mock.f[0x6400]=0x410;mock.failwrite=f;assert(!run(29)&&dispatch.failed&&dispatch.debug_disabled.address[0]==0x70000);unsigned n=mock.writes;assert(!run(29)&&mock.writes==n);}
 init();mock.f[0x4002]=0;assert(!run(29));init();mock.f[0x6820]=1u<<17;assert(!run(29));
 const unsigned fields[]={0x6400,0x6400,0x6400,0x681a,0x2802,0x6820,0x6804,0x4816,0x440c,0x440c};
 const uint64_t vals[]={4,5,8,0x401,1,256,0x22020,0xc09b,0,16};for(unsigned i=0;i<10;i++){init();mock.f[fields[i]]=vals[i];assert(!run(29)&&dispatch.failed&&!dispatch.ticks);}
 for(unsigned i=1;i<=18;i++){init();mock.failread=i;assert(!run(29));}
 init();session.native.gpr[0]=0x800000000000ULL;assert(!run(29)&&!dispatch.debug_disabled.address[0]);
 init();session.native.gpr[0]=0x1234;mock.failwrite=4;assert(!run(29)&&!dispatch.debug_disabled.address[0]);
 init();mock.f[0x6400]=6;session.native.gpr[0]=1;assert(!run(29)&&dispatch.failed);
 init();session.native.gpr[0]=0x1234;assert(run(29));struct lgs_session other=session;mock.f[0x4402]=29;
 assert(!lgd_dispatch(&dispatch,&other,29)&&dispatch.failed);session.prepared=false;session.memory.prepared=false;session.enabled=false;session.host.active=false;
 assert(lgd_reset(&dispatch)&&!dispatch.debug_disabled.address[0]&&!dispatch.debug_disabled.failed);
 struct lnp_state pf={.count=1,.pending=true},next;assert(lnp_observe(&pf,29,0,0,&next)&&!next.pending);assert(!lnp_observe(&pf,52,0,0,&next));
 struct lei_irq irqstate={.phase=LEI_STAGED,.armed=true};lei_observe(&irqstate,false,true,29,0);assert(irqstate.phase==LEI_DELIVERED);
 irqstate=(struct lei_irq){.phase=LEI_STAGED,.armed=true};lei_observe(&irqstate,false,true,52,0);assert(irqstate.phase==LEI_QUARANTINED);
 puts("linux_debug_disabled: all encoded GPRs, DR initialization/readback, active-debug reject, failures PASS (host only; NOT active breakpoint support)");return 0;}
