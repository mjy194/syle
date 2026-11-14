#include "user_fixture.c"
#define main original_tests
#include "fixture.c"
#undef main
static void user_dispatch_setup(void){
 user_setup();setup(true);memcpy(mock.f,user_fields,sizeof mock.f);
 session.memory=user_session.memory;session.host.active=session.host.ready=true;
 dispatch.pf=(struct lpfg_state){4,15,false,true};dispatch.high=LHP_CONSUMED;
}
static void consecutive_setup(void){
 user_dispatch_setup();assert(run(0)&&dispatch.demand_pf.pending);
 assert(lgd_before_entry(&dispatch,&session));
 struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=0};
 assert(lgd_after_entry(&dispatch,&session,&r));
 /* The guest has resolved its instruction mapping; it now faults on data.
  * Retain prior entry-error=20, with the hardware-cleared valid bit. */
 user_put(0x4000,0xe005);mock.f[0x4406]=6;mock.f[0x6400]=0x401050;
 mock.f[0x4016]=0xb0e;mock.f[0x440c]=7;
}
static void consecutive_tests(void){
 consecutive_setup();assert(run(0)&&dispatch.demand_pf.pending&&dispatch.demand_pf.count==2);
 assert(dispatch.demand_pf.history[0].error==20&&dispatch.demand_pf.history[1].error==6);
 assert(mock.f[0x4016]==0x80000b0e&&mock.f[0x4018]==6&&!mock.f[0x401a]);
 assert(mock.f[0x681e]==0x400000&&session.native.cr2==0x401050);
 /* Read-nonpresent can follow an owned write PF, without advancing RIP. */
 assert(lgd_before_entry(&dispatch,&session));
 struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=0};
 assert(lgd_after_entry(&dispatch,&session,&r));
 mock.f[0x4016]=0xb0e;mock.f[0x4406]=4;mock.f[0x6400]=0x402050;
 assert(run(0)&&dispatch.demand_pf.pending&&dispatch.demand_pf.count==3);
 assert(mock.f[0x4018]==4&&session.native.cr2==0x402050&&mock.f[0x681e]==0x400000);
 assert(lgd_before_entry(&dispatch,&session)); /* owner accepts exact error4 */
 /* No new permission bypass: present data, supervisor code and repeated
  * tuples still fail in the unchanged validator. */
 for(unsigned bad=0;bad<3;bad++){
  consecutive_setup();
  if(bad==0)user_put(0x4008,0xe007);
  if(bad==1)user_put(0x4000,0xe001);
  if(bad==2)dispatch.demand_pf.history[0]=(struct lnp_tuple){0x400000,0x401050,0x1000,6},mock.f[0x4018]=6;
  assert(!run(0)&&dispatch.failed&&dispatch.diagnostic_stage==LGD_STAGE_PF&&dispatch.demand_pf.count==1);
 }
 for(unsigned bad=0;bad<8;bad++){
  consecutive_setup();
  if(bad==0)mock.f[0x4016]=0x80000b0e;
  if(bad==1)mock.f[0x4016]=0;
  if(bad==2)mock.f[0x4018]=6;
  if(bad==3)mock.f[0x401a]=1;
  if(bad==4)mock.f[0x4408]=0x80000b0e;
  if(bad==5)mock.f[0x4404]=0x80000b08;
  if(bad==6)mock.f[0x4406]=7;
  if(bad==7)mock.f[0x4406]=5;
  unsigned writes_before=mock.writes;
  assert(!run(0)&&dispatch.failed&&dispatch.demand_pf.pending&&dispatch.demand_pf.count==1&&mock.writes==writes_before);
 }
 for(unsigned f=1;f<=4;f++){
  consecutive_setup();mock.failread=mock.reads+7+f;unsigned w=mock.writes;
  assert(!run(0)&&dispatch.demand_pf.pending&&dispatch.demand_pf.count==1&&mock.writes==w);
 }
 for(unsigned f=1;f<=6;f++){
  consecutive_setup();mock.failwrite=mock.writes+f;
  assert(!run(0)&&dispatch.failed&&dispatch.demand_pf.count==1);assert(!run(0));
 }
}
int main(void){
 consecutive_tests();
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

 user_dispatch_setup();write_setup(); /* refresh private RAM as user writable-fault fixture */
 memcpy(mock.f,user_fields,sizeof mock.f);assert(run(0)&&dispatch.demand_pf.pending&&dispatch.demand_pf.history[0].error==6&&session.native.cr2==0x401050);
 assert(lgd_before_entry(&dispatch,&session));r=(struct lgs_entry_result){.state_result=LVS_EXIT,.fields_valid=true,.reason=10};assert(lgd_after_entry(&dispatch,&session,&r));mock.f[0x0802]=0x10;mock.f[0x681e]=K+0xa000;mock.f[0x440c]=2;assert(run(10)&&!dispatch.demand_pf.pending);
 user_dispatch_setup();write_setup();memcpy(mock.f,user_fields,sizeof mock.f);assert(run(0));mock.f[0x4018]=14;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed);
 puts("actual dispatcher userPF injection/owner/receipt/failstop PASS");
}
