#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
#include "../linux_invlpg.h"
static void init(void){setup(false);session.memory.prepared=true;session.host.active=session.host.ready=true;
 session.memory.tables[0]=0x2000;session.memory.eptp=session.host.eptp=0x201e;
 mock.f[0x201a]=0x201e;mock.f[0x4002]=0x80000200;mock.f[0x401e]=0x42;
 mock.f[0x2806]=0x500;mock.f[0x4816]=0xa09b;mock.f[0x4012]=0x200;
 mock.f[0x6400]=0xffffffffff400000ULL;mock.f[0x440c]=3;}
int main(void){init();uint64_t g[15];memcpy(g,session.native.gpr,sizeof g);
 assert(run(14)&&mock.invs==1&&mock.f[0x681e]==0x10003&&dispatch.ticks==1&&!memcmp(g,session.native.gpr,sizeof g));
 init();mock.invfail=true;assert(!run(14)&&dispatch.failed&&mock.f[0x681e]==0x10000&&!dispatch.ticks);
 init();mock.failwrite=4;assert(!run(14)&&dispatch.failed&&mock.invs==1&&!dispatch.ticks&&!memcmp(g,session.native.gpr,sizeof g));
 unsigned writes=mock.writes;assert(!run(14)&&mock.writes==writes&&mock.invs==1);
 const unsigned fields[]={0x401e,0x4002,0x201a,0x6804,0x6804,0x0802,0x4816,0x2806,0x440c,0x440c,0x6400,0x681e};
 const uint64_t values[]={0x62,0x200,0x301e,0x22020,0x3020,0x13,0xc09b,0,0,16,0x800000000000ULL,0x7ffffffffffeULL};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++){init();mock.f[fields[i]]=values[i];assert(!run(14)&&dispatch.failed&&!mock.invs&&!dispatch.ticks);}
 for(unsigned i=1;i<=17;i++){init();mock.failread=i;assert(!run(14)&&!mock.invs);}
 init();mock.f[0x440c]=15;assert(run(14)&&mock.f[0x681e]==0x1000f);
 init();mock.f[0x4402]=13;assert(!linv_apply(&session,0x10000,0,3)&&!mock.invs);
 init();session.host.active=false;assert(!run(14)&&!mock.invs);
 init();session.host.eptp=0x301e;assert(!run(14)&&!mock.invs);
 init();session.memory.tables[0]=0x3000;assert(!run(14)&&!mock.invs);
 init();session.memory.prepared=false;assert(!run(14)&&!mock.invs);
 struct lnp_state pf={.count=1,.pending=true},next;assert(lnp_observe(&pf,14,0,0,&next)&&!next.pending);assert(!lnp_observe(&pf,52,0,0,&next));
 struct lei_irq irqstate={.phase=LEI_STAGED,.armed=true};lei_observe(&irqstate,false,true,14,0);assert(irqstate.phase==LEI_DELIVERED);
 irqstate=(struct lei_irq){.phase=LEI_STAGED,.armed=true};lei_observe(&irqstate,false,true,52,0);assert(irqstate.phase==LEI_QUARANTINED);
 puts("linux_invlpg_dispatch: no-VPID/EPT ownership, boundaries/read/flush/RIP failures PASS (host only)");return 0;}
