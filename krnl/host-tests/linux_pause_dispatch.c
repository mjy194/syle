#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
static void init(void){setup(false);mock.f[0x4002]=0xc0000000;mock.f[0x401e]=0x42;mock.f[0x4816]=0xa09b;mock.f[0x2806]=0x500;mock.f[0x4012]=0x200;}
int main(void){init();mock.f[0x6820]=0x202;uint64_t g[15];memcpy(g,session.native.gpr,sizeof g);assert(run(40)&&mock.f[0x681e]==0x10002&&dispatch.ticks==1&&!memcmp(g,session.native.gpr,sizeof g));assert(mock.f[0x6820]==0x202&&mock.f[0x4002]==0xc0000000&&mock.f[0x401e]==0x42);
 init();mock.failwrite=4;assert(!run(40)&&dispatch.failed&&!dispatch.ticks&&!memcmp(g,session.native.gpr,sizeof g));unsigned n=mock.writes;assert(!run(40)&&mock.writes==n);
 const unsigned fields[]={0x4002,0x401e,0x0802,0x4816,0x6804,0x4012,0x2806,0x440c,0x440c,0x681e};
 const uint64_t values[]={0x80000000,0x442,0x13,0xc09b,0x22020,0,0,1,3,0x7fffffffffffULL};
 for(unsigned i=0;i<10;i++){init();mock.f[fields[i]]=values[i];assert(!run(40)&&dispatch.failed&&!dispatch.ticks);}
 for(unsigned i=1;i<=16;i++){init();mock.failread=i;assert(!run(40));}
 init();assert(run(16));uint64_t start=session.native.gpr[0];unsigned loops=0;do{assert(run(40));assert(run(16));loops++;}while(session.native.gpr[0]-start<9000);assert(loops==5&&session.native.gpr[0]-start==10000&&dispatch.ticks==11);
 init();dispatch.config.max_exits=2;assert(run(40));uint64_t rip=mock.f[0x681e];assert(!run(40)&&dispatch.exits==2&&dispatch.ticks==1&&mock.f[0x681e]==rip);
 struct lnp_state pf={.count=1,.pending=true},next;assert(lnp_observe(&pf,40,0,0,&next)&&!next.pending);assert(!lnp_observe(&pf,52,0,0,&next));
 struct lei_irq irq={.phase=LEI_STAGED,.armed=true};lei_observe(&irq,false,true,40,0);assert(irq.phase==LEI_DELIVERED);irq=(struct lei_irq){.phase=LEI_STAGED,.armed=true};lei_observe(&irq,false,true,52,0);assert(irq.phase==LEI_QUARANTINED);
 puts("linux_pause: strict PAUSE control/mode, failstop, shared-time delay progress and unchanged budget PASS (host only)");return 0;}
