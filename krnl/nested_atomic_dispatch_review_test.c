/* Independent N41C stopped-consumer proof, real production dispatch policy. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main n41b_dispatch_existing_main
#include "tests/nested_domain_dispatch.c"
#undef main
#pragma GCC diagnostic pop
int main(void) {
 struct nested_domain_dispatch s;struct nvmx m[2];
 setup(&s,m);
 assert(nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
 assert(nested_domain_dispatch_consumer_stopped(&s,1,s.tokens[1]));
 for(unsigned cpu=0;cpu<2;cpu++)for(unsigned field=0;field<4;field++) {
  struct nested_domain_token bad=s.tokens[cpu];
  if(field==0)bad.domain_id++;
  if(field==1)bad.incarnation++;
  if(field==2)bad.slot++;
  if(field==3)bad.generation++;
  assert(!nested_domain_dispatch_consumer_stopped(&s,cpu,bad));
 }
 assert(!nested_domain_dispatch_consumer_stopped(&s,2,s.tokens[0]));
 start_ept(&s,m);
 assert(!nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
 for(unsigned round=0;round<4;round++) {
  assert(nested_domain_dispatch_physical_suspend_ept(&s,0,s.tokens[0],&m[0],0,48));
  assert(m[0].running&&!m[0].prepared&&!s.domain.cpus[0].guest_running);
  assert(nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
  assert(nested_domain_dispatch_physical_resume(&s,0,s.tokens[0],&m[0]));
  assert(!nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
 }
 assert(nested_domain_dispatch_physical_leave(&s,0,s.tokens[0],&m[0],true,0,18));
 assert(nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
 s.domain.objects[s.tokens[0].slot].generation++;
 assert(!nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
 setup(&s,m);s.domain.objects[s.tokens[0].slot].used=false;
 assert(!nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
 setup(&s,m);nested_domain_dispatch_markfail(&s);
 assert(!nested_domain_dispatch_consumer_stopped(&s,0,s.tokens[0]));
 puts("PASS review N41C stopped-consumer gate rejects stale/foreign/running/faulted dispatch and accepts four actual EPT suspends with model.running");
 return 0;
}
