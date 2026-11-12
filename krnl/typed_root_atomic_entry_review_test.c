/* Independent N41C entry review. All CPU/INVEPT hooks are host mocks. */
#define main n41b_fixture_existing_main
#include "typed_root_atomic_review_test.c"
#undef main

static unsigned late_mode;
static bool entry_stopped(void *p) {
 struct review_owner *o=p;
 assert(!lock_depth);
 if(o->atomic.entry_active) {
  switch(late_mode) {
  case 1:o->f->directory.poisoned=true;break;
  case 2:o->f->lease.poisoned=true;break;
  case 3:binding(o)->slot->directory.record.cache_generation++;break;
  case 4:o->registry.active->pins=0;break;
  case 5:o->registry.active->tree.poisoned=true;break;
  case 6:o->atomic.entry_operation.nonce++;break;
  case 7:o->f->directory.operations=0;break;
  case 8:o->f->directory.closed=true;break;
  case 9:o->f->directory.sealed=true;break;
  case 10:o->bridge.directory.busy=true;break;
  case 11:o->atomic.poisoned=true;break;
  }
 }
 return !o->running;
}
static void entry_positive(void) {
 struct review_fixture *f=fixture();
 for(unsigned cpu=0;cpu<2;cpu++) {
  struct review_owner *o=&f->owners[cpu];select_root(o,0);
  assert(tr_atomic_ready(&o->atomic));
  assert(!binding(o)->slot->directory.record.history);
  struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);
  assert(item&&item->pins==1);
  assert(tr_atomic_entry(&o->atomic,item,item->tree.eptp));
  assert(binding(o)->slot->directory.record.history==(UINT64_C(1)<<cpu));
  assert(!o->atomic.entry_active&&!o->atomic.entry_operation.live&&!f->directory.operations&&!f->directory.ops);
  struct ad_updater_token peer={0};uint64_t nonce=0;
  assert(ad_updater_gate_acquire(&f->gate,f->stamp,&peer,&nonce));
  assert(ad_updater_gate_open(&f->gate,f->stamp)&&!ad_updater_gate_idle(&f->gate,f->stamp));
  assert(tr_atomic_entry(&o->atomic,item,item->tree.eptp));
  assert(ad_updater_gate_release(&f->gate,&peer,f->stamp,nonce));
  assert(nested_ept_contexts_unpin(&o->registry,item));
 }
 clean(f);
 puts("PASS review N41C entry records each local residency, releases stable operation and tolerates active peer updater");
}
static void entry_corruption(void) {
 for(unsigned mode=0;mode<52;mode++) {
  struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
  struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);
  struct tr_atomic_binding *b=binding(o);struct tr_registry_slot *s=b->slot;
  uint64_t actual=item->tree.eptp;
  switch(mode) {
  case 0:physical_cpu=1;break;
  case 1:o->running=true;break;
  case 2:actual^=4096;break;
  case 3:item->pins=0;break;
  case 4:select_root(o,1);break;
  case 5:item->tree.hardware_ad=false;break;
  case 6:item->tree.generation++;break;
  case 7:item->transaction_token=NULL;break;
  case 8:item->key^=4096;break;
  case 9:s->directory.record.full_eptp^=4096;break;
  case 10:s->directory.record.hpa^=4096;break;
  case 11:s->directory.record.cache_generation++;break;
  case 12:s->directory.identity.root_id++;break;
  case 13:s->directory.identity.allocator_lifetime++;break;
  case 14:s->typed.allocator_lifetime++;break;
  case 15:s->typed.full_eptp^=4096;break;
  case 16:s->typed.hpa^=4096;break;
  case 17:s->typed.record=NULL;break;
  case 18:s->typed.token_live=false;break;
  case 19:f->lease.type_epoch++;break;
  case 20:o->atomic.poisoned=true;break;
  case 21:o->registry.poisoned=true;break;
  case 22:o->bridge.directory.faulted=true;break;
  case 23:f->lease.poisoned=true;break;
  case 24:f->directory.poisoned=true;break;
  case 25:assert(nrc_close(&f->catalog,f->id));break;
  case 26:assert(ad_updater_gate_close(&f->gate,f->stamp));break;
  case 27:o->atomic.config.stamp.epoch++;break;
  case 28:b->root.identity.domain++;break;
  case 29:b->root.identity.incarnation++;break;
  case 30:b->root.identity.catalog++;break;
  case 31:b->root.identity.lifetime++;break;
  case 32:b->root.source++;break;
  case 33:b->root.owner++;break;
  case 34:b->root.generation++;break;
  case 35:b->root.lifetime++;break;
  case 36:b->root.full_eptp^=4096;break;
  case 37:b->root.root_gpa^=4096;break;
  case 38:b->pin.root.lifetime++;break;
  case 39:b->pin_nonce++;break;
  case 40:b->released=true;break;
  case 41:b->bound=false;break;
  case 42:b->pin.authority_context=NULL;break;
  case 43:s->directory.record.pins++;break;
  case 44:s->directory.record.state=TD_RETAINED;break;
  case 45:f->directory.closed=true;break;
  case 46:f->directory.sealed=true;break;
  case 47:o->bridge.refill_operation=&o->atomic.fault.operation;break;
  case 48:o->bridge.directory.busy=true;break;
  case 49:o->atomic.entry_active=true;break;
  case 50:o->atomic.entry_operation.live=true;break;
  case 51:item->tree.poisoned=true;break;
  }
  unsigned frees=o->frees,flushes=o->flushes;
  bool result=tr_atomic_entry(&o->atomic,item,actual);
  if(result)fprintf(stderr,"entry corruption accepted mode=%u\n",mode);
  assert(!result);
  assert(o->frees==frees&&o->flushes==flushes&&!s->directory.record.history);
 }
 puts("PASS review N41C 52 entry CPU/consumer/pin/cache/source/typed/gate corruptions reject before residency");
}
static void entry_late_failure(void) {
 for(unsigned resident=0;resident<2;resident++)for(late_mode=1;late_mode<=11;late_mode++) {
  struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
  struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);
  o->atomic.config.consumer_stopped=entry_stopped;
  bool result=resident?tr_atomic_entry(&o->atomic,item,item->tree.eptp):tr_atomic_ready(&o->atomic);
  if(result)fprintf(stderr,"late entry failure accepted mode=%u resident=%u\n",late_mode,resident);
  assert(!result&&o->atomic.poisoned&&o->registry.poisoned&&f->lease.poisoned);
  if(late_mode==6||late_mode==7) {
   assert(o->atomic.entry_active&&o->atomic.entry_operation.live&&o->atomic.entry_nonce);
   assert(f->directory.ops==&o->atomic.entry_operation);
   assert(!tr_atomic_entry(&o->atomic,item,item->tree.eptp));
  }else assert(!o->atomic.entry_active&&!o->atomic.entry_operation.live&&!f->directory.ops);
 }
 puts("PASS review N41C late entry health/identity failures and durable failed-cleanup operation prevent reentry");
}
int main(int argc,char **argv) {
 const char *which=argc>1?argv[1]:"all";
 if(!strcmp(which,"all")||!strcmp(which,"positive"))entry_positive();
 if(!strcmp(which,"all")||!strcmp(which,"corruption"))entry_corruption();
 if(!strcmp(which,"all")||!strcmp(which,"late"))entry_late_failure();
 return 0;
}
