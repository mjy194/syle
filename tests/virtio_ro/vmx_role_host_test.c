/* Reuse the legacy VMX host mock fixture, then exercise the additive actual
 * lvh_open_roles path with the actual optional PMM authority. No VMX instruction. */
#define main legacy_host_test_main
#include "../../krnl/tests/linux_vcpu_host.c"
#undef main
#include "native_candidate.h"
static uint64_t epoch;
static unsigned memory_context_cookie;
static struct vlm_allocation allocations[2];
static bool wrong_kind,lifetime_pin,barrier_fail;
static struct vlm_handle held[2];
static bool pinned[2];
static unsigned barriers;
static bool partial_unpin;
static uint64_t retain_control;
static bool deny_on(void *ctx,uint64_t pa){(void)ctx;(void)pa;assert(!m.active);m.calls++;return false;}
static bool unpin_controls(void *ctx){
 assert(ctx==&epoch&&!c.active&&!m.active&&!c.ready);barriers++;
 if(barrier_fail)return false;
 for(unsigned i=0;i<2;i++)if(pinned[i]){if(!pna_unpin(epoch,held[i]))return false;pinned[i]=false;if(partial_unpin){partial_unpin=false;return false;}}
 return true;
}
static unsigned map_calls;
static uint64_t role_allocate(void *ctx,enum sg_memory_role role){
 assert(ctx==&memory_context_cookie);assert(role==(m.alloc?SG_MEMORY_VMCS:SG_MEMORY_VMXON));
 if(!tick())return 0;
 assert(m.alloc<2);unsigned i=m.alloc;uint64_t pa;
 assert(pna_alloc_typed(epoch,wrong_kind?VLM_GUEST_RAM:VLM_VMX_CONTROL,&allocations[i]));assert(pna_page(epoch,allocations[i],&pa)&&pa==0x100000+(uint64_t)i*4096);
 if(lifetime_pin){assert(pna_pin(epoch,allocations[i],&held[i]));pinned[i]=true;}
 m.alloc++;m.used[i]=true;m.live++;return pa;
}
static bool control(uint64_t pa){unsigned i=idx(pa);enum vlm_kind kind;uint64_t actual;return pna_page(epoch,allocations[i],&actual)&&actual==pa&&pna_kind(epoch,allocations[i],&kind)&&kind==VLM_VMX_CONTROL;}
static bool role_wb(void *ctx,uint64_t pa){return control(pa)&&wb(ctx,pa);}
static void *role_map(void *ctx,uint64_t pa){if(!control(pa))return 0;map_calls++;return map(ctx,pa);}
static bool role_release(void *ctx,uint64_t pa){unsigned i=idx(pa);if(m.block==4||pa==retain_control)return false;assert(!m.active);if(!pna_release(epoch,allocations[i]))return false;return release(ctx,pa);}
static void begin_case(void){reset();wrong_kind=false;map_calls=0;lifetime_pin=false;barrier_fail=false;barriers=0;partial_unpin=false;retain_control=0;assert(!pinned[0]&&!pinned[1]);assert(pna_start(&epoch));}
static void end_case(void){assert(!m.active&&!m.live&&!pmm_get_stats().allocated_pages);assert(pna_drain(epoch)&&pna_end(epoch));}
struct pdesc {uint32_t type,pad;uint64_t base,virt,pages,attr;};
static struct pdesc descriptor={7,0,0x100000,0,2,8};
int main(void){assert(!legacy_host_test_main());struct syle_boot_info b={0};b.magic=SYLE_BOOT_MAGIC;b.version=SYLE_BOOT_VERSION;b.size=sizeof(b);b.boot_services_exited=1;b.descriptor_size=sizeof(descriptor);b.descriptor_version=1;b.memory_map=(uintptr_t)&descriptor;b.memory_map_bytes=sizeof(descriptor);b.map_buffer=(struct syle_range){(uintptr_t)&descriptor,sizeof(descriptor)};assert(pmm_init(&b));
 ops.memory.context=&memory_context_cookie;ops.memory.allocate=0;ops.memory.release=role_release;ops.memory.map=role_map;ops.memory.writeback=role_wb;
 begin_case();assert(!lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,0)&&!m.calls);assert(lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate));unsigned steps=m.calls;assert(map_calls==2&&pmm_get_stats().allocated_pages==2);assert(lvh_close(&c));end_case();
 for(unsigned f=1;f<=steps;f++){begin_case();m.fail=f;assert(!lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate));end_case();}
 for(unsigned block=1;block<=4;block++){begin_case();assert(lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate));m.block=block;assert(!lvh_close(&c)&&m.live==2&&pmm_get_stats().allocated_pages==2);m.block=0;assert(lvh_close(&c));end_case();}
 begin_case();wrong_kind=true;assert(!lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate));assert(!map_calls);end_case();
 /* Lifetime pins block automatic legacy cleanup; only actual close-success
  * control flow permits unpin, never a caller-created quiesced flag. */
 begin_case();lifetime_pin=true;assert(lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate));
 m.block=3;assert(!lvh_close_with_barrier(&c,unpin_controls,&epoch)&&c.active&&!barriers&&pinned[0]&&pinned[1]);
 m.block=0;barrier_fail=true;assert(!lvh_close_with_barrier(&c,unpin_controls,&epoch)&&!c.active&&barriers==1&&m.live==2);
 unsigned closed_calls=m.calls;barrier_fail=false;assert(lvh_close_with_barrier(&c,unpin_controls,&epoch)&&m.calls==closed_calls);end_case();
 begin_case();lifetime_pin=true;m.fail=steps;
 assert(!lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate));
 assert(!c.active&&!m.active&&m.live==2&&pinned[0]&&pinned[1]);
 closed_calls=m.calls;assert(lvh_close_with_barrier(&c,unpin_controls,&epoch)&&m.calls==closed_calls);end_case();
 begin_case();lifetime_pin=true;assert(lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate));
 partial_unpin=true;assert(!lvh_close_with_barrier(&c,unpin_controls,&epoch)&&!pinned[0]&&pinned[1]&&m.live==2&&!c.active);
 closed_calls=m.calls;retain_control=c.vmxon_page;
 assert(!lvh_close_with_barrier(&c,unpin_controls,&epoch)&&!pinned[0]&&!pinned[1]&&m.live==1&&c.vmxon_page&&!c.vmcs_page&&m.calls==closed_calls);
 retain_control=0;assert(lvh_close_with_barrier(&c,unpin_controls,&epoch)&&m.calls==closed_calls);end_case();
 begin_case();lifetime_pin=true;ops.vmxon=deny_on;
 assert(!lvh_open_roles(&c,&ops,&ram,0x80000,0x400000,role_allocate)&&!c.active&&m.live==2);
 closed_calls=m.calls;assert(lvh_close_with_barrier(&c,unpin_controls,&epoch)&&m.calls==closed_calls);ops.vmxon=on;end_case();
 printf("actual VMXON/VMCS role sites + PMM: %u failure positions PASS (mock VMX only)\n",steps);return 0;}
