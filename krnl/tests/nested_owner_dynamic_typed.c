#include "../nested_owner_dynamic_typed.h"
#include "../paging.h"
#include "../vmx_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define DATA UINT64_C(0x800000)
#define SLOTS 16u
static _Alignas(4096) unsigned char pool_memory[96][4096];
static _Alignas(4096) uint64_t fixture_memory[63][512],fixture_tables[4][512];
static struct nested_owner_sparse owner;
static struct nested_owner_dynamic_typed state;
static struct ept_fixture fixture;
static struct typed_directory directory;
static struct typed_root_lease lease;
static struct typed_root_registry bridge;
static struct tr_registry_slot slots[SLOTS];
static struct {
 unsigned reserved,allocations,flushes,ready,fatals,depth,guards,resolve_views;
 uint64_t last_eptp,serial;
 bool alloc_fail,flush_fail,ready_fail,guard_fail,close_during_resolve;
} test;
static void lock(void *p){(void)p;assert(!test.depth);test.depth=1;}
static void unlock(void *p){(void)p;assert(test.depth==1);test.depth=0;}
void *paging_phys_to_virt(uint64_t hpa) {
 assert(!test.depth);
 if(test.close_during_resolve) {
  lock(NULL);assert(directory.operations==1&&bridge.refill_operation);
  assert(!trl_close(&lease)&&!directory.closed);unlock(NULL);
 }
 for(unsigned i=0;i<4;i++)if(hpa==fixture.tables[i])return fixture_tables[i];
 for(unsigned i=0;i<63;i++)if(hpa==fixture.pages[i]){
  assert(hpa!=DATA);test.resolve_views++;return fixture_memory[i];
 }
 return NULL;
}
int vmx_memory_region_wb(uint64_t hpa) {
 assert(!test.depth);
 for(unsigned i=0;i<4;i++)if(hpa==fixture.tables[i])return 1;
 for(unsigned i=0;i<63;i++)if(hpa==fixture.pages[i])return hpa!=DATA;
 return 0;
}
static bool guard(void *p){(void)p;assert(!test.depth);test.guards++;return !test.guard_fail;}
static void fatal(void *p){(void)p;test.fatals++;}
static bool reserve(void *p,struct tr_registry_slot **out,uint64_t *lifetime) {
 (void)p;assert(test.depth==1);
 if(test.reserved==SLOTS)return false;
 *out=&slots[test.reserved++];*lifetime=100+test.reserved;return true;
}
static bool evidence(void *p,struct tr_registry_slot *slot,struct td_retire_ticket ticket,
 uint64_t version,struct tr_registry_flush_evidence *out) {
 (void)p;assert(!test.depth&&test.last_eptp==ticket.full_eptp&&test.serial);
 lock(NULL);assert(slot->directory.item&&version==directory.version&&!slot->directory.item->pins);
 *out=(struct tr_registry_flush_evidence){ticket,version,ticket.history,test.serial};unlock(NULL);return true;
}
static bool alloc(void *p,struct nested_sparse_page *out) {
 struct nested_owner_sparse *s=p;assert(!test.depth&&(bridge.directory.busy||bridge.refill_operation));test.allocations++;
 return !test.alloc_fail&&nested_owner_pool_alloc(&s->pool,out);
}
static void release(void *p,struct nested_sparse_page page) {
 struct nested_owner_sparse *s=p;assert(!test.depth);nested_owner_pool_free(&s->pool,page);assert(!s->pool.fault);
}
static bool flush(void *p,uint64_t eptp) {
 (void)p;assert(!test.depth&&(eptp&4095)==0x1e);test.flushes++;
 if(test.flush_fail)return false;
 test.last_eptp=eptp;test.serial++;return true;
}
static const struct nested_sparse_ops ops={alloc,release,flush};
static bool ready(struct nested_owner_sparse *s) {
 assert(!test.depth&&s->contexts.count==1&&s->creations==1&&s->contexts.active->key==0x1001e);
 assert(state.bound&&s->dynamic_typed==&state&&s->contexts.evict&&s->contexts.max_resident==2);
 lock(NULL);assert(lease.live_tokens==1&&test.reserved==1);unlock(NULL);
 test.ready++;return !test.ready_fail;
}
static void target_type(unsigned type) {
 fixture_tables[3][15]=DATA|0x40|((uint64_t)type<<3)|3;
 for(unsigned i=0;i<3;i++)fixture_memory[18+4*i][15]=0xf000|0x40|((uint64_t)type<<3)|3;
}
static void configure(void) {
 memset(&owner,0,sizeof owner);memset(&state,0,sizeof state);memset(&fixture,0,sizeof fixture);
 memset(&test,0,sizeof test);memset(slots,0,sizeof slots);memset(&directory,0,sizeof directory);memset(&lease,0,sizeof lease);
 memset(&bridge,0,sizeof bridge);memset(fixture_memory,0,sizeof fixture_memory);memset(fixture_tables,0,sizeof fixture_tables);
 unsigned a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
 for(unsigned i=0;i<96;i++)owner.pages[i]=(struct nested_sparse_page){0x100000+4096*i,pool_memory[i]};
 assert(nested_owner_pool_init(&owner.pool,b>>24,1,owner.pages,owner.slots,96,48));
 for(unsigned i=0;i<4;i++)fixture.tables[i]=0x300000+4096*i;
 for(unsigned i=0;i<3;i++)fixture_tables[i][0]=fixture.tables[i+1]|7;
 for(unsigned i=0;i<63;i++){
  fixture.pages[i]=0x200000+4096*i;
  fixture_tables[3][i+1]=fixture.pages[i]|0x70|7;
 }
 fixture.pages[14]=DATA;
 for(unsigned i=0;i<3;i++) {
  unsigned root=15+4*i;
  for(unsigned j=0;j<3;j++)fixture_memory[root+j][0]=((uint64_t)(root+j+2)*4096)|7;
  for(unsigned j=1;j<64;j++)fixture_memory[root+3][j]=((uint64_t)j*4096)|0x70|7;
 }
 target_type(TRL_WB);
 assert(td_init(&directory,10,11,1,1)&&trl_init(&lease,&directory,(struct trl_backing){DATA,88,true},SLOTS));
 const struct tr_registry_provider provider={reserve,evidence};
 assert(typed_root_registry_init(&bridge,&lease,0,DATA,&provider,NULL));
 assert(typed_root_registry_set_sync(&bridge,lock,unlock,NULL));
 owner.multiple=owner.evict=owner.initialized=true;owner.fixture=&fixture;owner.selected12=0x1001e;
 state.bridge=&bridge;state.environment_guard=guard;state.fatal=fatal;
 owner.dynamic_typed=&state;owner.typed_transactions=typed_root_registry_transactions();
 owner.typed_transaction_context=&bridge;owner.contexts_ready=ready;
}
static void reject(void) {
 assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));
 assert(!test.allocations&&!test.reserved&&!owner.pool.live&&!test.ready&&!test.depth);
}
static void stop(void) {
 test.flush_fail=false;test.alloc_fail=false;test.close_during_resolve=false;
 assert(nested_owner_sparse_stop(&owner)&&!owner.pool.live&&owner.destroyed&&!lease.live_tokens&&!test.depth);
}
static void transition(unsigned destination) {
 assert(nested_owner_sparse_reset(&owner));lock(NULL);
 struct trl_freeze freeze;assert(trl_close(&lease)&&trl_seal(&lease,&freeze));
 uint64_t epoch=lease.type_epoch+1;
 for(struct trl_binding *b=lease.bindings;b;b=b->next)if(b->token_live) {
  struct trl_token old,prepared;assert(trl_token_read(&lease,b,&old));
  assert(trl_prepare_token(&lease,freeze,b,old,epoch,destination,&prepared));
 }
 target_type(destination);
 struct trl_transition_evidence proof={.freeze=freeze,.new_epoch=epoch,.serial=100+epoch,
  .closed=1,.drained=1,.tlb_before=1,.cache_before=1,.tlb_after=1,.cache_after=1,.destination=destination,
  .complete_alias_audit=true,.all_refills_revoked=true,.all_roots_invalidated=true,.host_break_before_make=true,
  .all_aliases_retyped=true,.owner_stamps_ready=true};
 assert(trl_reopen(&lease,freeze,&proof));unlock(NULL);
}
int main(void) {
 configure();owner.dynamic_typed=NULL;reject();
 configure();state.bridge=NULL;reject();
 configure();owner.contexts_ready=NULL;reject();
 configure();owner.typed_transactions=NULL;reject();
 configure();owner.typed_transaction_context=NULL;reject();
 configure();bridge.metadata_lock=NULL;reject();
 configure();bridge.metadata_unlock=NULL;reject();
 configure();state.environment_guard=NULL;reject();
 configure();state.fatal=NULL;reject();
 configure();owner.evict=false;reject();
 configure();assert(nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));
 assert(test.reserved==1&&test.allocations==3&&owner.pool.live==3&&test.ready==1);
 test.close_during_resolve=true;
 /* Actual fixed-fixture resolver, walker, admission and sparse fill. */
 assert(nested_owner_sparse_miss(&owner,0xf000,3));
 assert(test.resolve_views==4&&!directory.operations&&!bridge.refill_operation);
 assert(nested_owner_sparse_select(&owner,0x1401e)&&nested_owner_sparse_miss(&owner,0xf000,3));
 uint64_t original_root=slots[0].directory.record.hpa;
 transition(TRL_UC);
 assert(nested_owner_sparse_select(&owner,0x1801e)&&nested_owner_sparse_miss(&owner,0xf000,3));
 assert(slots[0].directory.forgotten&&slots[0].typed.state==TRL_RETIRED&&owner.contexts.count==2);
 assert(lease.type==TRL_UC&&test.reserved==3&&owner.evictions==1);
 transition(TRL_WB);
 assert(nested_owner_sparse_select(&owner,0x1001e)&&nested_owner_sparse_miss(&owner,0xf000,3));
 assert(test.reserved==4&&owner.evictions==2&&slots[3].directory.record.hpa==original_root);
 assert(slots[0].directory.identity.root_id!=slots[3].directory.identity.root_id);
 assert(!nested_owner_sparse_select(&owner,0x1c01e));
 unsigned allocations=test.allocations;
 assert(!nested_owner_sparse_miss(&owner,0xf000,1)&&state.failed&&test.fatals==1&&test.allocations==allocations);
 assert(!directory.operations&&!bridge.refill_operation);stop();
 configure();test.alloc_fail=true;assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));assert(!owner.pool.live&&test.reserved==1&&!lease.live_tokens);
 configure();test.flush_fail=true;assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));assert(owner.pool.live&&slots[0].typed.state==TRL_RETAINED);stop();
 configure();test.ready_fail=true;assert(!nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));assert(owner.pool.live==3&&state.bound);stop();
 configure();assert(nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));
 /* Referencing dedicated target as EPT12 table returns no readable view. */
 fixture_memory[15][0]=0xf000|7;
 assert(!nested_owner_sparse_miss(&owner,0xf000,1)&&state.failed&&!directory.operations&&!bridge.refill_operation);stop();
 configure();assert(nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));
 fixture_tables[3][15]&=~UINT64_C(0x40);
 assert(!nested_owner_sparse_miss(&owner,0xf000,1)&&!directory.operations&&!bridge.refill_operation);stop();
 puts("PASS dynamic typed owner actual bootstrap, fixed resolver, three-key eviction/recreation, whole-walk permit; mock flush only");return 0;
}
