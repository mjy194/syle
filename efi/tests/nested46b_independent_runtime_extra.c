static void late_guard_failures(void){
 for(unsigned held=0;held<2;held++)for(unsigned at=1;at<=3;at++){
  setup();late_fail_at=at;late_calls=0;domain_dispatch.lock=held;
  assert(!run(NONE,0));
  assert(atomic_runtime.memory.catalog.published&&atomic_runtime.memory.prepared&&domain_memory.atomic_ad_prepared);
  assert(!atomic_uc.released&&atomic_failed()&&domain_dispatch.fault_latch);
  assert(atomic_uc.controller.completed_actions==(at==1?39u:40u));
  assert(atomic_uc.controller.state==(at==1?NAUC_POISONED:NAUC_PUBLISHED));
  assert(late_calls==at);late_fail_at=0;domain_dispatch.lock=0;
  assert(atomic_uc_current_guard(NULL,0xf000,1));
  assert(nested_atomic_memory_ready(&atomic_runtime.memory));
  assert(!atomic_healthy(NULL)&&!atomic_uc_final());
  for(unsigned cpu=0;cpu<2;cpu++){
   struct nvmx model;assert(nvmx_init(&model,cpu?0x2d000:0xd000,cpu?0x2e000:0xe000,48));
   assert(!nested_domain_dispatch_physical_enter(&domain_dispatch,cpu,domain_dispatch.tokens[cpu],&model,false));
   assert(!nested_domain_dispatch_physical_enter(&domain_dispatch,cpu,domain_dispatch.tokens[cpu],&model,true));
   assert(!nested_domain_dispatch_physical_resume(&domain_dispatch,cpu,domain_dispatch.tokens[cpu],&model));
  }
  unsigned n=seen_actions;assert(!atomic_uc_bsp_provision());assert(seen_actions==n&&!atomic_uc.released);
  late_fail_at=late_calls=0;
 }
 puts("PASS 6 actual late guard failures after publication: adapter, BSP ready, BSP final inventory; permanent real dispatch latch with free or held metadata lock; restored host probe cannot reopen entry");
}
static void fail_latch_is_real(void){
 setup();struct nvmx model;assert(nvmx_init(&model,0xd000,0xe000,48));
 assert(nested_domain_dispatch_physical_enter(&domain_dispatch,0,domain_dispatch.tokens[0],&model,false));
 assert(nested_domain_guest_leave(&domain_dispatch.domain,(struct nested_domain_cpu_token){0x5333,1,0}));
 atomic_fail();assert(domain_dispatch.fault_latch&&atomic_failed());
 assert(!nested_domain_dispatch_physical_enter(&domain_dispatch,0,domain_dispatch.tokens[0],&model,false));
 puts("PASS otherwise admissible physical VMCS01 entry closes through the production atomic_fail and dispatch fault latch");
}
int main(void){int r=supplied_runtime_main();assert(!r);late_guard_failures();fail_latch_is_real();restore_target();puts("INDEPENDENT_RUNTIME_OK extra_cases=7 hardware_claim=false");return 0;}
