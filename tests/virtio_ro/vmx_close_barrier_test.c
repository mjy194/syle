#define main legacy_host_main
#include "../../krnl/tests/linux_vcpu_host.c"
#undef main
static unsigned barriers;
static bool barrier_fail;
static bool barrier(void *ctx){assert(ctx==&barriers);assert(!c.active&&!m.active&&!c.ready);barriers++;return !barrier_fail;}
int main(void){
 assert(!legacy_host_main());
 for(unsigned block=1;block<=3;block++){
  reset();barriers=0;assert(lvh_open(&c,&ops,&ram,0x80000,0x400000));
  unsigned calls=m.calls;assert(!lvh_close_with_barrier(&c,0,&barriers)&&m.calls==calls&&c.ready&&c.active);
  m.block=block;assert(!lvh_close_with_barrier(&c,barrier,&barriers)&&c.active&&!c.ready&&m.live==2&&!barriers);
  m.block=0;assert(lvh_close_with_barrier(&c,barrier,&barriers)&&barriers==1&&!m.live&&!c.active);
 }
 reset();barriers=0;assert(lvh_open(&c,&ops,&ram,0x80000,0x400000));
 barrier_fail=true;assert(!lvh_close_with_barrier(&c,barrier,&barriers)&&barriers==1&&!c.active&&!m.active&&m.live==2);
 unsigned calls=m.calls;assert(!lvh_close_with_barrier(&c,barrier,&barriers)&&barriers==2&&m.calls==calls&&m.live==2);
 barrier_fail=false;m.block=4;assert(!lvh_close_with_barrier(&c,barrier,&barriers)&&m.calls==calls&&m.live==2);
 m.block=0;assert(lvh_close_with_barrier(&c,barrier,&barriers)&&m.calls==calls&&!m.live);
 assert(lvh_close_with_barrier(&c,barrier,&barriers)&&m.calls==calls&&!m.live);
 reset();barriers=0;c.quarantined=true;assert(!lvh_close_with_barrier(&c,barrier,&barriers)&&!barriers&&!m.calls);c.quarantined=false;
 puts("actual lvh optional close barrier: VMX instruction failure, post-off barrier/free retry, quarantine and legacy PASS; mocked VMX");return 0;
}
