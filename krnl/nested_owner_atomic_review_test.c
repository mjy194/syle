/* Independent N41C actual source/owner/bridge/dispatch integration.
 * Host physical memory, CPU identity and INVEPT are deliberately mocked. */
#define main existing_memory_review_main
#include "../efi/tests/nested41c_memory.c"
#undef main
#include "nested_owner_atomic.h"
#include "nested_owner_sparse.h"
#include <stdlib.h>
static _Alignas(4096) unsigned char private_pages[NESTED_OWNER_POOL_PAGES][4096];
static unsigned actual_cpu,depth;
struct fixture {
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct ad_updater_gate gate;
 struct ad_entry_stamp stamp;
 struct nested_owner_sparse sparse;
 struct nested_owner_atomic owner;
 struct nested_domain_dispatch dispatch;
 struct nested_vcpu vcpu;
 struct ept_fixture fixture;
 uint64_t serial;
 unsigned allocations,frees,flushes,provider_flushes;
 bool flush_fail,provider_fail,source_release_fail,shared_failed;
};
static void meta_lock(void *p){(void)p;assert(!depth);depth=1;}
static void meta_unlock(void *p){(void)p;assert(depth==1);depth=0;}
static bool provider_lock(void *p){meta_lock(p);return true;}
static unsigned cpu(void *p){(void)p;return actual_cpu;}
static bool stopped(void *p){return nested_owner_atomic_stopped(&((struct fixture *)p)->owner);}
static bool healthy(void *p){struct fixture *f=p;return !f->owner.failed&&!f->shared_failed;}
static bool alloc_page(void *p,struct nested_sparse_page *page){
 struct fixture *f=p;assert(!depth);f->allocations++;return nested_owner_pool_alloc(&f->sparse.pool,page);
}
static void free_page(void *p,struct nested_sparse_page page){
 struct fixture *f=p;assert(!depth);
 for(unsigned n=0;n<f->owner.used;n++)if(f->owner.slots[n].directory.record.hpa==page.hpa) {
  struct tr_registry_slot *s=&f->owner.slots[n];
  assert(s->directory.forgotten&&s->directory.receipt.live&&s->typed.state==TRL_RETIRED);
  assert(f->owner.bindings[n].released&&!f->owner.bindings[n].pin.live);
  assert(tr_local_completed_matches(&f->owner.provider,s));
 }
 f->frees++;nested_owner_pool_free(&f->sparse.pool,page);assert(!f->sparse.pool.fault);
}
static bool flush_page(void *p,uint64_t eptp){
 struct fixture *f=p;assert(!depth&&(eptp&4095)==0x5e);f->flushes++;
 if(f->owner.atomic.fault.active) {
  struct tr_atomic_fault *q=&f->owner.atomic.fault;
  assert(q->permit.active&&q->permit.source.live&&q->permit.updater.live&&q->transient);
 }
 return !f->flush_fail;
}
static bool reserve_slot(void *p,struct tr_registry_slot **slot,uint64_t *life){
 struct fixture *f=p;assert(depth==1);if(f->owner.used==NESTED_ATOMIC_SLOTS)return false;
 *slot=&f->owner.slots[f->owner.used++];*life=1000+f->owner.used;return true;
}
static bool invalidate(void *p,uint64_t eptp,struct tr_local_invalidation *out){
 struct fixture *f=p;assert(!depth);f->provider_flushes++;
 if(f->provider_fail)return false;
 *out=(struct tr_local_invalidation){.full_eptp=eptp,.acked=1,.cpu=actual_cpu,.success=true};return true;
}
static bool evidence(void *p,struct tr_registry_slot *s,struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out){
 struct fixture *f=p;bool ok=tr_local_provider_flush(&f->owner.provider,s,scope,out);
 if(ok&&f->source_release_fail)m.catalog.lock=1;
 return ok;
}
static struct fixture *make(bool bootstrap){
 struct fixture *f=calloc(1,sizeof(*f));assert(f);actual_cpu=depth=0;
 base(&d);memset(&m,0,sizeof(m));assert(nested_atomic_memory_prepare(&m,&d,id));
 f->stamp=(struct ad_entry_stamp){id.domain,id.incarnation,id.catalog,1,id.lifetime};
 assert(ad_updater_gate_init(&f->gate,f->stamp));
 assert(td_init(&f->directory,id.domain,id.incarnation,3,1));
 assert(trl_init_atomic_wb(&f->lease,&f->directory,(struct trl_backing){d.backing.pages[14],1,true},NESTED_ATOMIC_SLOTS));
 assert(nested_domain_dispatch_init(&f->dispatch,id.domain,id.incarnation));
 assert(nested_vcpu_init(&f->vcpu,0,0x500000));
 assert(nested_vcpu_register02(&f->vcpu,0,0x501000));
 f->fixture=d.backing;for(unsigned l=0;l<4;l++)f->fixture.tables[l]=d.root01[0][l];
 f->fixture.eptp=f->fixture.tables[0]|0x1e;
 struct nested_owner_sparse *s=&f->sparse;
 for(unsigned n=0;n<NESTED_OWNER_POOL_PAGES;n++)s->pages[n]=(struct nested_sparse_page){0x100000+4096*n,private_pages[n]};
 assert(nested_owner_pool_init(&s->pool,0,1,s->pages,s->slots,NESTED_OWNER_POOL_PAGES,48));
 s->fixture=&f->fixture;s->initialized=true;s->multiple=true;s->evict=true;s->selected12=0x1005e;s->atomic_catalog=&f->owner;
 struct nested_owner_atomic *a=&f->owner;
 a->sparse=s;a->vcpu=&f->vcpu;a->dispatch=&f->dispatch;a->token=f->dispatch.tokens[0];a->lease=&f->lease;a->physical_ad=true;a->healthy=healthy;a->healthy_context=f;
 a->root_provider=(struct tr_registry_root_provider){reserve_slot,evidence};a->provider_context=f;
 a->config=(struct tr_atomic_config){.catalog=&m.catalog,.catalog_identity=id,.authority_context=&m,
  .gate=&f->gate,.stamp=f->stamp,.slots=a->slots,.bindings=a->bindings,.slot_count=NESTED_ATOMIC_SLOTS,
  .traces=a->traces,.trace_count=NESTED_ATOMIC_TRACES,.lock=meta_lock,.unlock=meta_unlock,.current_cpu=cpu,
  .consumer_stopped=stopped,.context=f};
 a->provider=(struct tr_local_provider){.bridge=&a->bridge,.slots=a->slots,.completed=a->completed,
  .slot_count=NESTED_ATOMIC_SLOTS,.owner=0,.serial=&f->serial,.lock=provider_lock,.unlock=meta_unlock,
  .current_cpu=cpu,.healthy=healthy,.invalidate=invalidate,.context=f};
 const struct nested_sparse_ops ops={alloc_page,free_page,flush_page};
 if(bootstrap)assert(nested_owner_atomic_bootstrap(s,&ops,f));
 return f;
}
static void pin(struct fixture *f){f->sparse.hardware_item=nested_ept_contexts_pin_active(&f->sparse.contexts);assert(f->sparse.hardware_item);}
static void unpin(struct fixture *f){assert(nested_ept_contexts_unpin(&f->sparse.contexts,f->sparse.hardware_item));f->sparse.hardware_item=NULL;}
static void select_key(struct fixture *f,uint64_t key){
 assert(nested_ept_contexts_select(&f->sparse.contexts,key)==NESTED_CONTEXT_OK);f->sparse.selected12=key;
}
static void positive(void){
 struct fixture *f=make(true);struct nested_owner_atomic *a=&f->owner;
 assert(f->sparse.contexts.hardware_ad&&f->sparse.contexts.max_resident==2&&f->sparse.creations==1);
 assert(nested_owner_atomic_entry(a,f->vcpu.vmcs01,f->fixture.eptp,false));pin(f);
 assert(nested_owner_atomic_entry(a,f->vcpu.vmcs02,f->sparse.hardware_item->tree.eptp,true));
 assert(nested_owner_atomic_fault(a,0xf000,0x181,0));
 uint64_t leaf;assert(nested_owner_atomic_leaf(a,0xf000,&leaf)&&!(leaf&2));
 assert((__atomic_load_n(&ram[18][15],__ATOMIC_ACQUIRE)&0x300)==0x100);
 assert(nested_owner_atomic_fault(a,0xf008,0x18a,0));
 assert(a->target_promotions[0]==1&&nested_owner_atomic_leaf(a,0xf000,&leaf)&&(leaf&2));
 unpin(f);select_key(f,0x1405e);pin(f);
 assert(nested_owner_atomic_fault(a,0xf000,0x181,0));
 assert(nested_owner_atomic_leaf(a,0xf000,&leaf)&&!(leaf&2));
 assert(nested_owner_atomic_fault(a,0xf018,0x18a,0));assert(a->target_promotions[1]==1);
 /* Writable guest paging pages are independent of immutable EPT12 pages. */
 assert(nested_owner_atomic_fault(a,0x1000,0x83,0));assert(a->paging_faults==1);
 unpin(f);select_key(f,0x1805e);pin(f);
 assert(nested_owner_atomic_fault(a,0xf000,0x181,0));
 assert(nested_owner_atomic_leaf(a,0xf000,&leaf)&&(leaf&~UINT64_C(4095))==d.backing.pages[27]);
 unpin(f);select_key(f,0x1005e);pin(f);
 assert(nested_owner_atomic_fault(a,0xf000,0x181,0));unpin(f);
 assert(nested_ept_contexts_destroy(&f->sparse.contexts));
 assert(f->owner.used==4&&f->provider_flushes==4&&!f->sparse.pool.live);
 for(unsigned n=0;n<a->used;n++)assert(tr_local_completed_matches(&a->provider,&a->slots[n]));
 assert(nrc_close(&m.catalog,id)&&nrc_drained(&m.catalog,id));
 puts("PASS review N41C production bootstrap/source/adapter/bridge A/B first writes, distinct C, pagewalk RW, durable four-root retirement");
}
static void gates(void){
 for(unsigned mode=0;mode<13;mode++) {
  struct fixture *f=make(true);pin(f);struct nested_owner_atomic *a=&f->owner;
  uint64_t vmcs=f->vcpu.vmcs02,eptp=f->sparse.hardware_item->tree.eptp;
  switch(mode){case 0:actual_cpu=1;break;case 1:f->vcpu.busy=true;break;
   case 2:f->dispatch.domain.cpus[0].guest_running=true;break;case 3:a->physical_ad=false;break;
   case 4:vmcs=f->vcpu.vmcs01;break;case 5:eptp^=0x40;break;case 6:f->vcpu.poison=true;break;
   case 7:f->sparse.selected12=0x1405e;break;case 8:f->sparse.hardware_item=NULL;break;
   case 9:assert(nrc_close(&m.catalog,id));break;case 10:a->bindings[0].root.lifetime++;break;
   case 11:ad_updater_gate_poison(&f->gate);break;case 12:f->shared_failed=true;break;}
  assert(!nested_owner_atomic_entry(a,vmcs,eptp,true));
  assert(!a->entries&&f->sparse.pool.live&&!f->frees);
 }
 puts("PASS review N41C adapter actual VMCS/EPTP, wrong CPU, running/busy, missing AD, closed/source-binding guards");
}
static void qualification(void){
 const uint64_t invalid[]={0,0x101,0x2181,0x8181,0x10181,0x1181,0x189,0x192,0x81,0x82,0x18a};
 for(unsigned n=0;n<sizeof(invalid)/sizeof(*invalid);n++) {
  struct fixture *f=make(true);pin(f);
  uint64_t original=ram[18][15];unsigned flushes=f->flushes;
  bool ok=nested_owner_atomic_fault(&f->owner,0xf000,invalid[n],0);
  if(ok)fprintf(stderr,"invalid qualification accepted index=%u q=%llx\n",n,(unsigned long long)invalid[n]);
  assert(!ok&&f->owner.failed&&ram[18][15]==original&&f->flushes==flushes&&!f->frees);
  assert(!nested_owner_atomic_entry(&f->owner,f->vcpu.vmcs02,f->sparse.hardware_item->tree.eptp,true));
 }
 struct fixture *f=make(true);pin(f);
 assert(nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));
 assert(!nested_owner_atomic_fault(&f->owner,0xf000,0x189,0)); /* unexplained read HIT */
 assert(f->owner.failed&&f->sparse.pool.live&&!f->frees);
 puts("PASS review N41C qualification errors and unexplained HIT fail closed before source write/publication");
}
/* The raw qualification is hardware information, not a guest permission
 * grant. Decode only the architecturally meaningful fields for this profile.
 * Enumerate undefined/unused metadata independently of actual source/bridge
 * permission checks, through the real production adapter. */
static uint64_t ignored_metadata(unsigned n){
 return ((n&1)?UINT64_C(1)<<6:0)|((uint64_t)((n>>1)&7)<<9)|
  ((n&16)?UINT64_C(1)<<14:0)|((n&32)?~UINT64_C(0x1ffff):0);
}
static void ignored_qualification(void){
 for(unsigned n=0;n<64;n++)for(unsigned mode=0;mode<4;mode++) {
  struct fixture *f=make(true);pin(f);struct nested_owner_atomic *a=&f->owner;
  uint64_t extra=ignored_metadata(n),leaf=0;
  if(mode==0||mode==3) {
   uint32_t control=mode==3?8:0;
   if(mode==3)extra|=0x1000; /* NMI-exiting=1, virtual-NMIs=0: undefined. */
   assert(nested_owner_atomic_fault(a,0x1000,0x83|extra,control));
   assert(a->paging_faults==1&&a->fills==1&&!a->promotions);
   assert(nested_owner_atomic_leaf(a,0x1000,&leaf)&&(leaf&3)==3);
  }else {
   uint64_t address_bits=mode==1?0x180:0;
   assert(nested_owner_atomic_fault(a,0xf000,address_bits|1|extra,0));
   assert(nested_owner_atomic_leaf(a,0xf000,&leaf)&&(leaf&7)==1);
   assert(nested_owner_atomic_fault(a,0xf008,address_bits|0x0a|extra,0));
   assert(nested_owner_atomic_leaf(a,0xf000,&leaf)&&(leaf&3)==3);
   assert(a->fills==1&&a->promotions==1&&a->target_promotions[0]==1&&!a->paging_faults);
   assert((__atomic_load_n(&ram[18][15],__ATOMIC_ACQUIRE)&0x300)==0x300);
  }
  assert(!a->failed&&!a->rejection.valid&&!a->atomic.fault.active&&!a->atomic.poisoned);
  unpin(f);assert(nested_ept_contexts_destroy(&f->sparse.contexts));
  assert(!f->sparse.pool.live&&nrc_close(&m.catalog,id)&&nrc_drained(&m.catalog,id));free(f);
 }
 puts("PASS review N41C 256 real-adapter ignored-metadata cases: bits6/9-11/14/17-63, GLA/noGLA, read/write promotion, paging RW, conditional NMI bit12");
}
static void rejection_latch(void){
 struct invalid_case {uint64_t q;uint32_t pin;enum nested_atomic_reject_stage stage;};
 const struct invalid_case invalid[]={
  {0,0,NOA_REJECT_ACCESS},{0x2181,0,NOA_REJECT_UNSUPPORTED},
  {0x8181,0,NOA_REJECT_UNSUPPORTED},{0x10181,0,NOA_REJECT_UNSUPPORTED},
  {0x1181,0,NOA_REJECT_UNSUPPORTED},{0x1181,32,NOA_REJECT_UNSUPPORTED},
  {0x1181,40,NOA_REJECT_UNSUPPORTED},{0x101,0,NOA_REJECT_GLA},
  {0x81,0,NOA_REJECT_PAGEWALK},{0x82,0,NOA_REJECT_PAGEWALK},
  {0x189,0,NOA_REJECT_ALLOWED},{0x192,0,NOA_REJECT_ALLOWED},
  {0x18a,0,NOA_REJECT_PERMISSIONS}
 };
 for(unsigned n=0;n<sizeof(invalid)/sizeof(*invalid);n++) {
  struct fixture *f=make(true);pin(f);struct nested_owner_atomic *a=&f->owner;
  uint64_t q=invalid[n].q|ignored_metadata(63),before=ram[18][15];
  unsigned flushes=f->flushes,allocations=f->allocations;
  assert(!nested_owner_atomic_fault(a,0xf008,q,invalid[n].pin));
  struct nested_atomic_rejection *r=&a->rejection;
  assert(__atomic_load_n(&r->valid,__ATOMIC_ACQUIRE)==1&&r->reason==48);
  assert(r->gpa==0xf008&&r->qualification==q&&r->pin_controls==invalid[n].pin);
  assert(r->stage==invalid[n].stage&&r->bridge_status==-1);
  assert(r->permissions_valid==(r->stage==NOA_REJECT_PERMISSIONS)&&r->actual_permissions==0);
  assert(a->failed&&!a->fills&&!a->promotions&&!a->atomic.fault.active&&!a->atomic.poisoned);
  assert(f->flushes==flushes&&f->allocations==allocations&&ram[18][15]==before&&!f->frees);
  struct nested_atomic_rejection saved=*r;
  assert(!nested_owner_atomic_fault(a,0x1234,0x181,8));
  assert(!memcmp(r,&saved,sizeof(saved))); /* First failure is immutable. */
  assert(!nested_owner_atomic_entry(a,f->vcpu.vmcs02,f->sparse.hardware_item->tree.eptp,true));
 }
 /* An existing RO private mapping must still match the reported EPT bits. */
 struct fixture *f=make(true);pin(f);assert(nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));
 assert(!nested_owner_atomic_fault(&f->owner,0xf000,0x182|ignored_metadata(63),0));
 assert(f->owner.rejection.stage==NOA_REJECT_PERMISSIONS&&f->owner.rejection.permissions_valid&&
        f->owner.rejection.actual_permissions==1&&f->owner.rejection.bridge_status==-1);
 /* Real tree resolution failure is distinct from a valid all-zero permission. */
 f=make(true);pin(f);f->sparse.contexts.active->tree.eptp=0xdead005e;
 assert(!nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));
 assert(f->owner.rejection.valid&&f->owner.rejection.stage==NOA_REJECT_PRIVATE_TREE&&
        !f->owner.rejection.permissions_valid&&f->owner.rejection.bridge_status==-1);
 /* Bridge failure retains its exact status and known pre-bridge permissions. */
 f=make(true);pin(f);denied_host=d.backing.pages[15];
 assert(!nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));
 assert(f->owner.rejection.valid&&f->owner.rejection.stage==NOA_REJECT_BRIDGE&&
        f->owner.rejection.permissions_valid&&f->owner.rejection.actual_permissions==0&&
        f->owner.rejection.bridge_status==TR_ATOMIC_FATAL);
 /* Bridge input rejection is distinct from poisoning after a failed source guard. */
 f=make(true);pin(f);
 assert(!nested_owner_atomic_fault(&f->owner,UINT64_C(1)<<48,0x181,0));
 assert(f->owner.rejection.valid&&f->owner.rejection.stage==NOA_REJECT_BRIDGE&&
        f->owner.rejection.bridge_status==TR_ATOMIC_REJECTED&&!f->owner.atomic.poisoned);
 /* Physical running state denies before admission, even if virtual model says stopped. */
 f=make(true);pin(f);f->dispatch.domain.cpus[0].guest_running=true;
 assert(!nested_owner_atomic_fault(&f->owner,0x1000,0x83,0));
 assert(f->owner.rejection.valid&&f->owner.rejection.stage==NOA_REJECT_STOPPED&&
        f->owner.rejection.bridge_status==-1&&!f->owner.atomic.fault.active&&!f->owner.fills);
 /* A diagnostic record is never authority or a substitute failure latch. */
 f=make(true);pin(f);
 f->owner.rejection=(struct nested_atomic_rejection){.gpa=0x9999,.qualification=UINT64_MAX,
  .reason=48,.stage=NOA_REJECT_STOPPED,.bridge_status=-1,.valid=1};
 struct nested_atomic_rejection saved=f->owner.rejection;
 assert(nested_owner_atomic_fault(&f->owner,0x1000,0x83,0));
 assert(!memcmp(&f->owner.rejection,&saved,sizeof(saved))&&!f->owner.failed);
 puts("PASS review N41C defined access/unsupported/GLA/pagewalk/denied/private/permission/bridge/stopped rejections and durable raw first-failure diagnostics");
}
static void failures(void){
 for(unsigned mode=0;mode<4;mode++) {
  struct fixture *f=make(true);pin(f);
  assert(nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));
  size_t live=f->sparse.pool.live;unsigned freed=f->frees;uint64_t eptp=f->sparse.contexts.active->tree.eptp;
  if(mode==0){f->flush_fail=true;assert(!nested_owner_atomic_fault(&f->owner,0xf000,0x18a,0));}
  if(mode==1){f->owner.traces[0].trace.source.lifetime++;assert(!nested_owner_atomic_fault(&f->owner,0xf000,0x18a,0));}
  if(mode>=2){unpin(f);f->provider_fail=mode==2;f->source_release_fail=mode==3;assert(!nested_ept_contexts_destroy(&f->sparse.contexts));}
  assert(f->sparse.pool.live==live&&f->frees==freed);
  assert(!nested_owner_atomic_entry(&f->owner,f->vcpu.vmcs02,eptp,true));
  if(mode==3){assert(f->owner.slots[0].directory.receipt.live&&f->owner.bindings[0].pin.live);m.catalog.lock=0;}
 }
 puts("PASS review N41C adapter stale trace/flush/provider/source-release failures retain private/source ownership and deny reentry");
}
static void authority_failures(void){
 for(unsigned mode=0;mode<5;mode++) {
  struct fixture *f=make(true);pin(f);uint64_t original=ram[18][15];
  if(mode==0)denied_host=d.backing.pages[15];
  if(mode==1)denied_wb=d.backing.pages[15];
  if(mode==2)ram[d.root01[1][3]/4096-1][16]|=2;
  if(mode==3)ram[d.root02[1][3]/4096-1][80]=d.backing.pages[15]|0x73;
  if(mode==4)ram[d.backing.tables[3]/4096-1][90]=d.backing.pages[15]|0x73;
  assert(!nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));
  assert(f->owner.failed&&ram[18][15]==original&&!f->frees&&f->sparse.pool.live);
 }
 puts("PASS review N41C missing hostRW/WB authority and writable source aliases in active/spare/retained roots reject before CAS");
}
static void bootstrap_failures(void){
 for(unsigned mode=0;mode<8;mode++) {
  struct fixture *f=make(false);
  if(mode==0)actual_cpu=1;
  if(mode==1)f->owner.physical_ad=false;
  if(mode==2)f->vcpu.busy=true;
  if(mode==3)f->dispatch.domain.cpus[0].guest_running=true;
  if(mode==4)f->shared_failed=true;
  if(mode==5)f->sparse.selected12=0x1001e;
  if(mode==6)f->sparse.multiple=false;
  if(mode==7)assert(nrc_close(&m.catalog,id));
  const struct nested_sparse_ops ops={alloc_page,free_page,flush_page};
  assert(!nested_owner_atomic_bootstrap(&f->sparse,&ops,f));
  assert(!f->allocations&&!f->frees&&!f->sparse.contexts.active&&!f->owner.used);
 }
 puts("PASS review N41C bootstrap denies 8 CPU/capability/busy/consumer/health/profile/source failures before private allocation");
}
int main(int argc,char **argv){
 const char *test=argc>1?argv[1]:"all";
 if(!strcmp(test,"all")||!strcmp(test,"positive"))positive();
 if(!strcmp(test,"all")||!strcmp(test,"gates"))gates();
 if(!strcmp(test,"all")||!strcmp(test,"qualification"))qualification();
 if(!strcmp(test,"all")||!strcmp(test,"ignored"))ignored_qualification();
 if(!strcmp(test,"all")||!strcmp(test,"diagnostic"))rejection_latch();
 if(!strcmp(test,"all")||!strcmp(test,"failures"))failures();
 if(!strcmp(test,"all")||!strcmp(test,"authority"))authority_failures();
 if(!strcmp(test,"all")||!strcmp(test,"bootstrap"))bootstrap_failures();
 return 0;
}
