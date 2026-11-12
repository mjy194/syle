#define main old_tests
#include "linux_guest_dispatch.c"
#undef main
static void legacy(void){setup(false);dispatch.config.legacy_pic_enabled=true;session.host.caps.primary=UINT64_C(0xffffffff00000000);mock.f[0x4002]=0x80000080;mock.f[0x401e]=0x42;mock.f[0x4816]=0xa09b;mock.f[0x2806]=0xd01;mock.f[0x4012]=0x200;assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};assert(lgd_after_entry(&dispatch,&session,&r));}
static bool output(unsigned p,unsigned v){session.native.gpr[0]=v;mock.f[0x6400]=((uint64_t)p<<16)|64;mock.f[0x440c]=2;return run(30);}
static bool separate_write(void*p,uint32_t f,uint64_t v){assert(p==&session);return wr(&mock,f,v);}
int main(void){old_tests();legacy();assert(output(0x21,251));assert(output(0x20,17));assert(output(0x21,48));assert(output(0x21,4));assert(output(0x21,1));assert(output(0x21,254));assert(output(0x43,0x34));assert(output(0x40,3));assert(output(0x40,0));assert(run(52));assert(run(52));assert(run(52));assert(run(52));assert(dispatch.legacy.timer.pic.irr);mock.f[0x6820]=512;assert(lgd_before_entry(&dispatch,&session));assert(dispatch.legacy.timer.pic.reserved);struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=10};assert(lgd_after_entry(&dispatch,&session,&r));assert(dispatch.legacy.timer.pic.isr);session.native.gpr[0]=1;assert(run(10));assert(output(0x20,0x20));assert(!dispatch.legacy.timer.pic.isr);
 legacy();mock.f[0x440c]=1;mock.f[0x6820]=512;assert(run(12)&&mock.f[0x4826]==1);assert(lgd_before_entry(&dispatch,&session));r=(struct lgs_entry_result){.state_result=LVS_ENTRY_FAILED};assert(lgd_after_entry(&dispatch,&session,&r));
 legacy();uint64_t ticks=dispatch.ticks;mock.failwrite=mock.writes+4;assert(!output(0x43,0x34)&&dispatch.failed&&dispatch.ticks==ticks&&!dispatch.legacy.timer.timer.mode);
 legacy();uint64_t gen=dispatch.generation;session.prepared=false;assert(lgd_reset(&dispatch)&&dispatch.generation==gen+1&&!dispatch.legacy_initialized);
 legacy();r=(struct lgs_entry_result){.state_result=LVS_GUEST_CAPTURE_FAILED};assert(lgd_before_entry(&dispatch,&session));assert(!lgd_after_entry(&dispatch,&session,&r)&&dispatch.failed);
 legacy();session.hop.context=&session;session.hop.vmwrite=separate_write;assert(output(0x21,255));assert(lgd_before_entry(&dispatch,&session));r=(struct lgs_entry_result){.state_result=LVS_ENTRY_FAILED};assert(lgd_after_entry(&dispatch,&session,&r));
 setup(false);struct lgd_config c=dispatch.config;c.legacy_pic_enabled=true;c.irq_platform_ready=true;struct lgd_context fresh={0};struct lgm_service_ops ops={0};assert(!lgd_init(&fresh,&c,&ops));
 legacy();dispatch.generation=UINT64_MAX;session.prepared=false;assert(!lgd_reset(&dispatch));
 for(unsigned mode=0;mode<2;mode++)for(unsigned bad=0;bad<7;bad++){
  legacy();if(mode){assert(output(0x20,17));assert(output(0x21,48));assert(output(0x21,4));assert(output(0x21,1));assert(output(0x21,254));assert(pic0_edges(&dispatch.legacy.timer.pic,dispatch.generation,1));}
  if(bad==0)mock.f[0x4016]=0x80000030;
  if(bad==1)mock.f[0x4408]=0x80000020;
  if(bad>=2){dispatch.pf=(struct lpfg_state){1,1,true,false};mock.f[0x4016]=0x80000b0e;mock.f[0x4018]=2;}
  if(bad==2)mock.f[0x4016]=0x80000030;
  if(bad==3)mock.f[0x4018]=0;
  if(bad==4)mock.f[0x401a]=1;
  if(bad==5)dispatch.high=LHP_PENDING;
  if(bad==6)mock.f[0x4408]=1;
  unsigned writes=mock.writes;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed&&mock.writes==writes);
 }
 for(unsigned owner=0;owner<3;owner++){legacy();mock.f[0x4016]=0x80000b0e;
  if(owner==0){dispatch.pf=(struct lpfg_state){1,1,true,false};mock.f[0x4018]=2;}
  if(owner==1){dispatch.pf=(struct lpfg_state){4,15,false,true};dispatch.high=LHP_PENDING;}
  if(owner==2){dispatch.pf=(struct lpfg_state){4,15,false,true};dispatch.high=LHP_CONSUMED;dispatch.demand_pf.pending=true;dispatch.demand_pf.count=1;dispatch.demand_pf.history[0].error=2;mock.f[0x4018]=2;}
  assert(lgd_before_entry(&dispatch,&session)&&!dispatch.legacy.timer.pic.reserved);
 }
 puts("legacy dispatcher v2 owned event matrix PASS");}
