/* Independent source-bridge test with the real typed registry and private pool. */
#define main inherited_registry_tests
#include "typed_root_registry_test.c"
#undef main
struct source_review {
 struct fixture *fixture;
 unsigned guards,translates,fail_guard;
 enum nested_ept_status translation_status;
 uint64_t lifetime,expected_lifetime;
};
static bool checked_guard(void *context,const struct tr_registry_walk *walk) {
 struct source_review *s=context;struct fixture *f=s->fixture;
 assert(!f->lock_depth&&f->directory.operations==1&&f->bridge.refill_operation);
 assert(f->bridge.refill_slot&&f->bridge.refill_slot->directory.record.refills==1);
 assert(!trl_close(&f->lease)&&!f->directory.closed);
 assert(walk->virtual_key==0x1001e&&walk->backing_hpa==DATA&&walk->type==TRL_WB&&walk->type_epoch==1);
 s->guards++;
 return s->lifetime==s->expected_lifetime&&s->guards!=s->fail_guard;
}
static struct nested_ept_result checked_translate(void *context,const struct tr_registry_walk *walk,uint64_t gpa,uint8_t access) {
 struct source_review *s=context;struct fixture *f=s->fixture;
 assert(s->guards==1&&!f->lock_depth&&f->directory.operations==1);
 assert(f->bridge.refill_operation&&walk->virtual_key==0x1001e&&gpa==0xf000&&access==3);
 s->translates++;
 return (struct nested_ept_result){.status=s->translation_status,.l1_gpa=0xf000,.hpa=DATA,
  .effective12=3,.effective02=3,.leaf_resolved=true,.memory_explicit=true,.ignore_pat=true,.leaf_type=6,.walk_type=6};
}
static const struct tr_registry_source checked_source={checked_guard,checked_translate};
static void run(unsigned fail_guard,enum nested_ept_status result) {
 struct fixture f;init(&f,PAGES,SLOTS);synchronize(&f);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 struct source_review s={&f,0,0,fail_guard,result,91,91};
 size_t before=f.pool.live;unsigned allocated=f.allocations;struct nested_ept_result out;
 enum nested_sparse_fill status=typed_root_registry_miss_source(&f.bridge,f.registry.active,0xf357,3,&checked_source,&s,&out);
 assert(!f.directory.operations&&!f.bridge.refill_operation&&!f.bridge.refill_slot);
 if(fail_guard==1){
  assert(status==NESTED_SPARSE_INVALID&&out.status==NESTED_EPT_UNSUPPORTED);
  assert(!s.translates&&f.pool.live==before&&f.allocations==allocated);
 }else if(fail_guard==2){
  assert(status==NESTED_SPARSE_INVALID&&out.status==NESTED_EPT_L0_BACKING_DENIED);
  assert(s.translates==1&&f.pool.live==before&&f.allocations==allocated);
 }else if(fail_guard==3){
  assert(status==NESTED_SPARSE_POISONED&&out.status==NESTED_EPT_L0_BACKING_DENIED);
  assert(f.bridge.directory.faulted&&f.lease.poisoned&&f.pool.live>before&&!f.frees);
  return; /* fatal path retains every tree and backing, deliberately no destroy */
 }else if(result!=NESTED_EPT_OK){
  assert(status==NESTED_SPARSE_INVALID&&out.status==result);
  assert(s.translates==1&&f.pool.live==before&&f.allocations==allocated);
 }else{
  assert(status==NESTED_SPARSE_OK&&out.status==NESTED_EPT_OK&&s.guards==3&&s.translates==1);
  before=f.pool.live;allocated=f.allocations;s.guards=s.translates=0;
  assert(typed_root_registry_miss_source(&f.bridge,f.registry.active,0xf000,3,&checked_source,&s,&out)==NESTED_SPARSE_INVALID);
  assert(s.guards==1&&!s.translates&&f.pool.live==before&&f.allocations==allocated);
  s.guards=s.translates=0;s.lifetime++; /* external generation guard also runs for warm lookup */
  assert(typed_root_registry_miss_source(&f.bridge,f.registry.active,0xf000,3,&checked_source,&s,&out)==NESTED_SPARSE_INVALID);
  assert(s.guards==1&&!s.translates&&f.pool.live==before&&f.allocations==allocated);
 }
 assert(!f.bridge.directory.faulted&&!f.lease.poisoned);
 assert(nested_ept_contexts_destroy(&f.registry));
}
int main(void) {
 for(unsigned f=0;f<=3;f++)run(f,NESTED_EPT_OK);
 run(0,NESTED_EPT_L1_DENIED);run(0,NESTED_EPT_L0_BACKING_DENIED);run(0,NESTED_EPT_UNSUPPORTED);
 puts("PASS independent N40A typed-source full permit, guarded warm lookup, fault separation and retained late failure");
}
