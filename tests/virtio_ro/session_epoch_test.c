#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main fixture_main
#define lgm_release fixture_lgm_release
#define lvh_close fixture_lvh_close
#include "../../krnl/tests/linux_guest_session.c"
#undef main
#undef lgm_release
#undef lvh_close
#pragma GCC diagnostic pop
#include "../../krnl/linux_guest_role_binding.h"
#include "session_authority.h"
static struct vsa a;
static struct vtc_entry entries[4];
static uint64_t ram,ept,on,cs;
static bool end_fail;
static unsigned page_calls,fail_page_call;
bool fi_pna_page(uint64_t epoch,struct vlm_allocation token,uint64_t *pa){
 if(++page_calls==fail_page_call)return false;
 return pna_page(epoch,token,pa);
}
static unsigned ends;
static struct guest_control ctl;
static bool begin_epoch(void *ctx){
 assert(guest_control_call(&ctl,7,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBUSY);
 assert(guest_control_owner_exit(&ctl,7)==-SYLE_GUEST_EBUSY);
 return vsa_begin(ctx);
}
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,0x100000,0,4,8};
bool lgm_prepare_roles(struct linux_guest_memory*m,const struct sg_memory_ops*o,sg_allocate_role_fn role,const void*k,size_t ks,const void*i,size_t is,const char*c){
 assert(o->context==&a&&role==vsa_allocate);ram=role(o->context,SG_MEMORY_RAM);ept=role(o->context,SG_MEMORY_EPT);if(!ram||!ept)return false;return lgm_prepare(m,o,k,ks,i,is,c);
}
bool lvh_open_roles(struct lvh_context*h,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip,sg_allocate_role_fn role){
 on=role(o->memory.context,SG_MEMORY_VMXON);cs=role(o->memory.context,SG_MEMORY_VMCS);assert(on&&cs);return lvh_open(h,o,m,rsp,rip);
}
bool lvh_close(struct lvh_context*h){if(!fixture_lvh_close(h))return false;if(cs){assert(vsa_release(&a,cs));cs=0;}if(on){assert(vsa_release(&a,on));on=0;}return true;}
bool lgm_release(struct linux_guest_memory*m){assert(!s.host.active);if(ept){assert(vsa_release(&a,ept));ept=0;}if(ram){if(!vsa_release(&a,ram))return false;ram=0;}return fixture_lgm_release(m);}
static bool end_epoch(void *ctx){ends++;assert(!ram&&!ept&&!on&&!cs&&!s.enabled&&!s.host.active);return !end_fail&&vsa_end(ctx);}
/* This fixture exercises the no-barrier configuration only. */
bool lvh_close_with_barrier(struct lvh_context *h,bool (*hook)(void *),void *ctx){
 (void)h;(void)hook;(void)ctx;assert(!"barrier requires actual lvh test");return false;
}
int main(int argc,char **argv){
 (void)argv;assert(argc==1||argc==2);
 struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof(bi);bi.boot_services_exited=1;bi.descriptor_size=sizeof(desc);bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof(desc);bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof(desc)};assert(pmm_init(&bi));
 a.storage=entries;a.capacity=4;
 const char asset='x';struct lvh_ops h={.memory={.context=&a,.release=vsa_release},.vmwrite=write,.invept=invalidate};struct lvs_ops v={0};struct lgs_platform p={.enable=enable,.disable=disable,.safe=safe,.read_field=rd,.dispatch=dispatch,.fatal=fatal};struct lgr_binding b={0};
 assert(lgs_configure(&s,&h,&v,&p,&asset,1,&asset,1,"",0x1000,0x1000));assert(lgr_configure(&b,&s,&a,vsa_allocate,begin_epoch,end_epoch));
 guest_control_init(&ctl,7,&lgr_guest_backend,&b);
 assert(guest_control_call(&ctl,8,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EPERM);
 if(argc==2){
  fail_page_call=2;
  assert(guest_control_call(&ctl,7,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBACKEND);
  assert(a.owned.failed&&a.owned.ended&&!b.active&&ctl.state==SYLE_GUEST_EMPTY);
  assert(!pmm_get_stats().allocated_pages&&ends==1&&!on&&!cs&&!ram&&!ept);
  puts("controller failed admission after published RAM: real PMM quarantine/end cleanup PASS");return 0;
 }
 assert(guest_control_call(&ctl,7,SYLE_GUEST_CREATE,0,1)==1&&pmm_get_stats().allocated_pages==4);
 enum vlm_kind kind;assert(vsa_query(&a,ram,&kind)&&kind==VLM_GUEST_RAM);assert(vsa_query(&a,ept,&kind)&&kind==VLM_EPT_TABLE);
 assert(ctl.state==SYLE_GUEST_READY&&pmm_get_stats().allocated_pages==4&&!ends);
 f.close_fail=1;assert(guest_control_call(&ctl,7,SYLE_GUEST_REAP,0,0)&&pmm_get_stats().allocated_pages==4&&!ends);
 f.close_fail=0;
 /* Failed admission is terminal for allocation, not for proven published frees. */
 assert(vtc_pin_ram(&a.owned,ram));a.owned.failed=true;assert(guest_control_call(&ctl,7,SYLE_GUEST_REAP,0,0)&&pmm_get_stats().allocated_pages==1&&!ends);
 assert(guest_control_call(&ctl,7,SYLE_GUEST_CREATE,0,1)<0&&guest_control_call(&ctl,7,SYLE_GUEST_START,1,0)<0);assert(vtc_unpin_ram(&a.owned,ram));
 end_fail=true;assert(guest_control_call(&ctl,7,SYLE_GUEST_REAP,0,0)&&!pmm_get_stats().allocated_pages&&ends==1&&b.base_released);
 unsigned closes=f.closes;end_fail=false;assert(!guest_control_call(&ctl,7,SYLE_GUEST_REAP,0,0)&&a.owned.ended&&ends==2&&f.closes==closes);
 /* Context is intentionally one-shot; stale callback identity cannot be reset. */
 assert(guest_control_call(&ctl,7,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBACKEND&&!b.active&&!pmm_get_stats().allocated_pages);
 assert(!guest_control_call(&ctl,7,SYLE_GUEST_REAP,0,0));
 puts("actual session + actual PMM epoch/role allocation + mocked LGM/VMX: close/pin/end failure retention PASS; no mapping or native execution");
}
