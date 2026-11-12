#include "user_fixture.c"
#define main original_tests
#include "fixture.c"
#undef main
static void user_dispatch_setup(void){
 user_setup();setup(true);memcpy(mock.f,user_fields,sizeof mock.f);
 session.memory=user_session.memory;session.host.active=session.host.ready=true;
 dispatch.pf=(struct lpfg_state){4,15,false,true};dispatch.high=LHP_CONSUMED;
}
int main(void){
 original_tests();user_model_tests();
 user_dispatch_setup();assert(run(0)&&dispatch.demand_pf.pending&&dispatch.demand_pf.count==1&&dispatch.ticks==1&&session.native.cr2==0x400000);
 assert(mock.f[0x4018]==20&&mock.f[0x4016]==0x80000b0e&&mock.f[0x681e]==0x400000);
 assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=10};assert(lgd_after_entry(&dispatch,&session,&r));
 mock.f[0x0802]=0x10;mock.f[0x681e]=K+0xa000;mock.f[0x440c]=2;session.native.gpr[0]=0;assert(run(10)&&!dispatch.demand_pf.pending);
 for(unsigned i=0;i<4;i++){user_dispatch_setup();assert(run(0));if(i==0)mock.f[0x4018]=2;if(i==1)mock.f[0x401a]=1;if(i==2)mock.f[0x4408]=1;if(i==3)dispatch.msr_gp.pending=true;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed);}
 user_dispatch_setup();mock.failwrite=4;assert(!run(0)&&dispatch.failed&&!dispatch.ticks&&!dispatch.demand_pf.pending&&!session.native.cr2);assert(!run(0));
 user_dispatch_setup();dispatch.demand_pf.count=31;for(unsigned i=0;i<31;i++)dispatch.demand_pf.history[i]=(struct lnp_tuple){0x1000+i,0x2000+i,0x1000,0};assert(run(0)&&dispatch.demand_pf.count==32&&dispatch.demand_pf.pending);
 user_dispatch_setup();dispatch.demand_pf.count=32;assert(!run(0)&&dispatch.failed);
 user_dispatch_setup();assert(!run(10)&&dispatch.failed);
 puts("actual dispatcher userPF injection/owner/receipt/failstop PASS");
}
