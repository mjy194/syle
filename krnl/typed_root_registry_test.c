#include "typed_root_registry.h"
#include "nested_owner_pool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PAGES 64u
#define SLOTS 24u
#define DATA UINT64_C(0x800000)
static _Alignas(4096) unsigned char memory[PAGES][4096];
enum evidence_fault { EVIDENCE_OK,EVIDENCE_FAIL,EVIDENCE_EPOCH,EVIDENCE_VERSION,EVIDENCE_ID,EVIDENCE_HISTORY };
struct fixture {
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct typed_root_registry bridge;
 struct nested_ept_contexts registry;
 struct nested_owner_pool pool;
 struct nested_sparse_page pages[PAGES];
 struct nested_owner_slot pool_slots[PAGES];
 struct tr_registry_slot slots[SLOTS];
 unsigned reserved,allocations,flush_calls,frees,lock_depth,lock_calls;
 bool synchronized,fail_alloc,change_version;
 size_t peak;
 uint64_t serial,last_flushed,fail_root,remote_flushed;
 bool fail_reserve,fail_flush,close_during_init,close_during_retire;
 enum evidence_fault evidence_fault;
};
static void metadata_lock(void *context) {
 struct fixture *f=context;assert(!f->lock_depth);f->lock_depth=1;f->lock_calls++;
}
static void metadata_unlock(void *context) {
 struct fixture *f=context;assert(f->lock_depth==1);f->lock_depth=0;
}
static void synchronize(struct fixture *f) {
 assert(typed_root_registry_set_sync(&f->bridge,metadata_lock,metadata_unlock,f));
 f->synchronized=true;
}
static bool alloc_page(void *context,struct nested_sparse_page *out) {
 struct fixture *f=context;
 /* Every registry candidate reserves both stable ledgers before allocation. */
 assert(f->bridge.directory.busy||f->bridge.refill_operation);
 assert(f->reserved&&f->lease.live_tokens);
 assert(!f->lock_depth);f->allocations++;
 if(f->fail_alloc)return false;
 bool ok=nested_owner_pool_alloc(&f->pool,out);
 if(f->pool.live>f->peak)f->peak=f->pool.live;
 return ok;
}
static void free_page(void *context,struct nested_sparse_page p) {
 struct fixture *f=context;assert(!f->lock_depth);
 for(unsigned i=0;i<f->reserved;i++) {
  struct tr_registry_slot *s=&f->slots[i];
  /* A bound root page can be freed only after durable directory AND typed
   * retirement. Historical proof stays valid when the pool later reuses HPA. */
  if(s->directory.record.attached&&s->directory.record.hpa==p.hpa) {
   assert(s->directory.forgotten&&s->directory.receipt.live);
   assert(s->typed.state==TRL_RETIRED&&!s->typed.token_live);
   assert(s->receipt_version&&s->typed.receipt_version==s->receipt_version);
  }
 }
 f->frees++;nested_owner_pool_free(&f->pool,p);assert(!f->pool.fault);
}
static bool mock_physical_flush(void *context,uint64_t eptp) {
 struct fixture *f=context;assert(!f->lock_depth);f->flush_calls++;
 assert((eptp&4095)==0x1e);
 if(f->fail_flush||eptp==f->fail_root)return false;
 f->last_flushed=eptp;f->serial++;
 if(f->close_during_init) {
  f->close_during_init=false;
  assert(!trl_close(&f->lease)&&!f->directory.closed);
  struct trl_freeze freeze;assert(!trl_seal(&f->lease,&freeze));
 }
 return true;
}
static bool reserve_slot(void *context,struct tr_registry_slot **slot,uint64_t *lifetime) {
 struct fixture *f=context;
 assert(!f->synchronized||f->lock_depth==1);
 if(f->fail_reserve||f->reserved==SLOTS)return false;
 *slot=&f->slots[f->reserved++];*lifetime=100+f->reserved;return true;
}
static bool flush_evidence(void *context,struct tr_registry_slot *slot,
 struct td_retire_ticket ticket,uint64_t version,struct tr_registry_flush_evidence *out) {
 struct fixture *f=context;
 assert(!f->lock_depth);if(f->synchronized)metadata_lock(f);
 /* This is mock physical-flush evidence. Actual INVEPT is never executed. */
 assert(f->last_flushed==ticket.full_eptp&&f->serial&&slot->directory.item);
 assert(slot->directory.item->tree.root&&f->pool.live);
 assert(slot->typed.token_live&&!slot->directory.receipt.live);
 if(f->close_during_retire) {
  assert(!trl_close(&f->lease));assert(!f->directory.closed);
 }
 if(f->evidence_fault==EVIDENCE_FAIL){if(f->synchronized)metadata_unlock(f);return false;}
 *out=(struct tr_registry_flush_evidence){.ticket=ticket,.directory_version=version,
  .acked=ticket.history&(UINT64_C(1)|f->remote_flushed),.flush_serial=f->serial};
 if(f->evidence_fault==EVIDENCE_EPOCH)out->ticket.type_epoch++;
 if(f->evidence_fault==EVIDENCE_VERSION)out->directory_version++;
 if(f->evidence_fault==EVIDENCE_ID)out->ticket.identity.allocator_lifetime++;
 if(f->evidence_fault==EVIDENCE_HISTORY)out->acked^=1;
 if(f->change_version){f->directory.version++;f->change_version=false;}
 if(f->synchronized)metadata_unlock(f);
 return true;
}
static void init(struct fixture *f,size_t pages,size_t slots) {
 memset(f,0,sizeof *f);
 assert(pages<=PAGES&&slots<=SLOTS);
 for(unsigned i=0;i<PAGES;i++)f->pages[i]=(struct nested_sparse_page){0x100000+4096*i,memory[i]};
 assert(nested_owner_pool_init(&f->pool,0,7,f->pages,f->pool_slots,pages,48));
 assert(td_init(&f->directory,10,11,3,1));assert(trl_init(&f->lease,&f->directory,(struct trl_backing){DATA,88,true},slots));
 const struct tr_registry_provider provider={reserve_slot,flush_evidence};
 assert(typed_root_registry_init(&f->bridge,&f->lease,0,DATA,&provider,f));
 const struct nested_sparse_ops ops={alloc_page,free_page,mock_physical_flush};
 assert(nested_ept_contexts_init_budget(&f->registry,&ops,f,48,2,true));
 assert(nested_ept_contexts_set_transactions(&f->registry,typed_root_registry_transactions(),&f->bridge));
}
static struct tr_registry_slot *active_slot(struct fixture *f) {
 for(unsigned i=0;i<f->reserved;i++)if(&f->slots[i].directory==f->registry.active->transaction_token)return &f->slots[i];
 assert(false);return NULL;
}
static void history(struct fixture *f,unsigned cpu) {
 struct tr_registry_slot *s=active_slot(f);struct td_operation op;uint64_t nonce;
 assert(td_begin(&f->directory,&op,&nonce));
 assert(td_resident(&f->directory,&op,nonce,&s->directory.record,s->directory.identity,cpu));
 assert(td_end(&f->directory,&op,nonce));
}
static struct nested_ept_result mapping(unsigned type,uint64_t hpa) {
 return (struct nested_ept_result){.status=NESTED_EPT_OK,.l1_gpa=0x8000,.hpa=hpa,
  .effective12=3,.effective02=3,.leaf_resolved=true,.memory_explicit=true,
  .leaf_type=type,.walk_type=6,.ignore_pat=true};
}
static struct trl_freeze freeze(struct fixture *f) {
 struct trl_freeze frozen;assert(trl_close(&f->lease)&&trl_seal(&f->lease,&frozen));return frozen;
}
static void transition(struct fixture *f,struct trl_freeze frozen,unsigned destination) {
 /* PURE attestations exercise the software protocol. This test does not
  * claim to change any host PTE, cache type, CPU stamp, or hardware state. */
 uint64_t epoch=f->lease.type_epoch+1;
 for(unsigned i=0;i<f->reserved;i++)if(f->slots[i].typed.token_live) {
  struct trl_token old,prepared;
  assert(trl_token_read(&f->lease,&f->slots[i].typed,&old));
  assert(trl_prepare_token(&f->lease,frozen,&f->slots[i].typed,old,epoch,destination,&prepared));
 }
 struct trl_transition_evidence evidence={.freeze=frozen,.new_epoch=epoch,.serial=1000+epoch,
  .closed=3,.drained=3,.tlb_before=3,.cache_before=3,.tlb_after=3,.cache_after=3,
  .destination=destination,.complete_alias_audit=true,.all_refills_revoked=true,.all_roots_invalidated=true,
  .host_break_before_make=true,.all_aliases_retyped=true,.owner_stamps_ready=true};
 assert(trl_reopen(&f->lease,frozen,&evidence));
}
static void clean(struct fixture *f) {
 f->fail_flush=false;f->fail_root=0;f->evidence_fault=EVIDENCE_OK;f->remote_flushed=2;
 nested_ept_contexts_release_all_after_clear(&f->registry);
 assert(nested_ept_contexts_destroy(&f->registry)&&!f->pool.live&&!f->lease.live_tokens);
 assert(!f->bridge.directory.faulted&&!f->directory.poisoned&&!f->lease.poisoned);
}
static void eviction_and_refill(void) {
 struct fixture f;init(&f,PAGES,SLOTS);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 struct tr_registry_slot *a=active_slot(&f);struct trl_token old_a;
 assert(trl_token_read(&f.lease,&a->typed,&old_a));uint64_t old_hpa=a->directory.record.hpa;
 struct nested_ept_result wb=mapping(TRL_WB,DATA),out;
 assert(nested_ept_sparse_fill(&f.registry.active->tree,0x1000,&wb)==NESTED_SPARSE_INVALID);
 f.close_during_init=true; /* close also refuses the admitted whole-fill permit */
 assert(typed_root_registry_fill(&f.bridge,f.registry.active,0x1000,&wb)==NESTED_SPARSE_OK);
 assert(!f.directory.closed);
 assert(typed_root_registry_lookup(&f.bridge,f.registry.active,0x1000,1,&out)==NESTED_SPARSE_HIT);
 assert(nested_ept_sparse_lookup(&f.registry.active->tree,0x1000,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
 assert(!a->directory.record.refills&&!f.directory.operations);
 history(&f,0);
 struct nested_ept_context_item *pinned=nested_ept_contexts_pin_active(&f.registry);
 assert(pinned&&a->directory.record.pins==1);
 assert(nested_ept_contexts_select(&f.registry,0x1101e)==NESTED_CONTEXT_OK);
 unsigned before_pin_test=f.allocations;
 assert(nested_ept_contexts_select(&f.registry,0x1201e)==NESTED_CONTEXT_LIMIT&&f.allocations==before_pin_test);
 assert(nested_ept_contexts_unpin(&f.registry,pinned)&&!a->directory.record.pins);
 struct tr_registry_slot *b=active_slot(&f);struct nested_ept_context_item *b_item=f.registry.active;
 history(&f,1);f.remote_flushed=2;
 assert(nested_ept_contexts_invalidate(&f.registry,2,0)==NESTED_CONTEXT_OK);
 struct trl_freeze frozen=freeze(&f);size_t first_index=99;
 assert(trl_snapshot_index(&f.lease,frozen,a->directory.identity,false,&first_index));
 unsigned allocations=f.allocations;
 assert(nested_ept_contexts_select(&f.registry,0x1201e)!=NESTED_CONTEXT_OK&&f.allocations==allocations);
 assert(typed_root_registry_fill(&f.bridge,b_item,0x1000,&wb)==NESTED_SPARSE_INVALID);
 transition(&f,frozen,TRL_UC);
 struct td_operation op;uint64_t nonce;assert(td_begin(&f.directory,&op,&nonce));
 assert(!trl_admit(&f.lease,&op,nonce,&a->typed,old_a,TRL_WB,true,false));
 assert(td_end(&f.directory,&op,nonce));
 struct nested_ept_result uc=mapping(TRL_UC,DATA),foreign_uc=mapping(TRL_UC,DATA+4096),ordinary_wb=mapping(TRL_WB,DATA+4096);
 struct nested_ept_result pat0=uc,wt=uc,executable=uc;pat0.ignore_pat=false;wt.leaf_type=4;
 executable.effective12=executable.effective02=7;
 assert(typed_root_registry_fill(&f.bridge,b_item,0x1000,&pat0)==NESTED_SPARSE_INVALID);
 assert(typed_root_registry_fill(&f.bridge,b_item,0x1000,&wt)==NESTED_SPARSE_INVALID);
 assert(typed_root_registry_fill(&f.bridge,b_item,0x1000,&executable)==NESTED_SPARSE_INVALID);
 assert(typed_root_registry_fill(&f.bridge,b_item,0x1000,&wb)==NESTED_SPARSE_INVALID);
 assert(typed_root_registry_fill(&f.bridge,b_item,0x1000,&foreign_uc)==NESTED_SPARSE_INVALID);
 ordinary_wb.effective12=ordinary_wb.effective02=5; /* unrelated owned WB code remains legal */
 assert(typed_root_registry_fill(&f.bridge,b_item,0x2000,&ordinary_wb)==NESTED_SPARSE_OK);
 assert(typed_root_registry_fill(&f.bridge,b_item,0x1000,&uc)==NESTED_SPARSE_OK);
 assert(typed_root_registry_lookup(&f.bridge,b_item,0x1000,3,&out)==NESTED_SPARSE_HIT&&out.leaf_type==TRL_UC);
 /* Third real root arrives while UC, evicting A only after receipt+token retirement. */
 assert(nested_ept_contexts_select(&f.registry,0x1201e)==NESTED_CONTEXT_OK);
 struct tr_registry_slot *c=active_slot(&f);
 assert(f.registry.count==2&&a->directory.forgotten&&a->typed.state==TRL_RETIRED);
 assert(a->typed.retirement.type_epoch==2&&a->typed.receipt_version==a->receipt_version);
 assert(f.lease.type==TRL_UC&&f.lease.live_tokens==2&&c->typed.token.type==TRL_UC);
 assert(typed_root_registry_fill(&f.bridge,f.registry.active,0x1000,&uc)==NESTED_SPARSE_OK);
 assert(nested_ept_contexts_invalidate(&f.registry,2,0)==NESTED_CONTEXT_OK);
 frozen=freeze(&f);size_t receipt_index=99;
 assert(trl_snapshot_index(&f.lease,frozen,a->directory.identity,true,&receipt_index));
 assert(receipt_index!=first_index); /* snapshots reorder; identities do not. */
 struct trl_snapshot snapshots[SLOTS*2];size_t count=0;
 assert(trl_snapshot_read(&f.lease,frozen,snapshots,SLOTS*2,&count)&&count==3);
 assert(!snapshots[receipt_index].scan_allowed&&snapshots[receipt_index].receipt_type_epoch==2);
 assert(snapshots[receipt_index].receipt_version==a->receipt_version);
 transition(&f,frozen,TRL_WB);
 /* Rebuild A after pool reuse. Complete identity, not HPA, distinguishes it. */
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 struct tr_registry_slot *new_a=active_slot(&f);
 assert(new_a->directory.record.hpa==old_hpa&&new_a->directory.identity.root_id!=old_a.identity.root_id);
 assert(new_a->directory.identity.allocator_lifetime!=old_a.identity.allocator_lifetime);
 assert(td_begin(&f.directory,&op,&nonce));
 assert(!trl_admit(&f.lease,&op,nonce,&new_a->typed,old_a,TRL_WB,true,false));
 assert(td_end(&f.directory,&op,nonce));
 assert(b->directory.forgotten&&b->directory.receipt.history==2&&b->directory.receipt.acked==2);
 clean(&f);
}
static void allocation_failures(void) {
 struct fixture f;
 for(size_t pages=6;pages<9;pages++) {
  init(&f,pages,SLOTS);
  assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
  assert(nested_ept_contexts_select(&f.registry,0x1101e)==NESTED_CONTEXT_OK);
  struct nested_ept_context_item *active=f.registry.active;
  assert(nested_ept_contexts_select(&f.registry,0x1201e)==NESTED_CONTEXT_OOM);
  assert(f.registry.active==active&&f.registry.count==2&&f.pool.live==6&&f.peak==pages);
  assert(f.lease.live_tokens==2&&!f.registry.poisoned&&f.slots[2].directory.forgotten);
  clean(&f);
 }
 init(&f,9,2);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 assert(nested_ept_contexts_select(&f.registry,0x1101e)==NESTED_CONTEXT_OK);
 unsigned allocations=f.allocations;
 assert(nested_ept_contexts_select(&f.registry,0x1201e)==NESTED_CONTEXT_OOM&&f.allocations==allocations);
 assert(f.reserved==2&&f.lease.reserved==2&&f.pool.live==6);clean(&f);
 init(&f,9,SLOTS);f.fail_reserve=true;
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OOM&&!f.allocations&&!f.pool.live);
 f.fail_reserve=false;clean(&f);
 init(&f,9,SLOTS);f.fail_flush=true;
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(f.pool.live==3&&f.registry.count==1&&f.slots[0].directory.record.cache_generation==0);
 assert(f.slots[0].typed.token_live&&f.slots[0].typed.state==TRL_RETAINED);
 clean(&f);
}
static void retirement_failures(void) {
 struct fixture f;
 init(&f,9,SLOTS);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 struct tr_registry_slot *a=active_slot(&f);history(&f,0);
 assert(nested_ept_contexts_select(&f.registry,0x1101e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item *active=f.registry.active;f.fail_root=a->directory.record.full_eptp;
 assert(nested_ept_contexts_select(&f.registry,0x1201e)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(f.registry.count==3&&f.pool.live==9&&f.registry.active==active&&f.lease.live_tokens==3);
 assert(!a->directory.receipt.live&&!a->directory.forgotten);clean(&f);
 for(enum evidence_fault failure=EVIDENCE_FAIL;failure<=EVIDENCE_HISTORY;failure++) {
  init(&f,9,SLOTS);
  assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);history(&f,0);
  f.evidence_fault=failure;f.close_during_retire=true;
  assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live==3&&f.lease.live_tokens==1);
  assert(f.slots[0].directory.record.state==TD_RETIRING&&!f.slots[0].directory.receipt.live);
  assert(!trl_close(&f.lease)&&!f.directory.closed);
  unsigned flushes=f.flush_calls;clean(&f);assert(f.flush_calls>flushes);
 }
 init(&f,9,SLOTS);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);history(&f,0);history(&f,1);
 assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live==3&&f.lease.live_tokens==1);
 assert(!f.slots[0].directory.receipt.live); /* local mock never invents remote ACK */
 clean(&f);assert(f.slots[0].directory.receipt.acked==3);
}
static void committed_retirement_failures(void) {
 struct fixture f;init(&f,9,SLOTS);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 /* Directory receipt commits, but exhausted typed version cannot release
  * the token. Both actual tree and stable receipt remain owned on retries. */
 f.lease.version=UINT64_MAX;
 assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live==3&&f.lease.live_tokens==1);
 struct tr_registry_slot *s=&f.slots[0];
 assert(s->directory.record.state==TD_RETIRED&&s->directory.receipt.live&&!s->directory.forgotten);
 uint64_t version=s->receipt_version;unsigned frees=f.frees,flushes=f.flush_calls;
 assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live==3&&f.frees==frees&&f.flush_calls>flushes);
 assert(s->receipt_version==version&&s->typed.token_live&&!trl_close(&f.lease));
 init(&f,9,SLOTS);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 /* Typed retirement succeeds; td_forget then fails at directory overflow.
  * Retry must not retire the token twice or authorize actual tree free. */
 f.directory.version=UINT64_MAX-2;
 assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live==3&&!f.lease.live_tokens);
 s=&f.slots[0];assert(s->typed.state==TRL_RETIRED&&s->directory.receipt.live&&!s->directory.forgotten);
 assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live==3&&!f.lease.live_tokens);
 struct trl_freeze frozen=freeze(&f);struct trl_snapshot snapshot[2];size_t count;
 assert(trl_snapshot_read(&f.lease,frozen,snapshot,2,&count)&&count==2);
 assert(!snapshot[0].scan_allowed&&!snapshot[1].scan_allowed);
 struct trl_final_evidence evidence={.freeze=frozen,.serial=999,.parked=3,.vmcs_gone=3,.refills_revoked=3,
  .roots_flushed=3,.caches_flushed=3,.restored_wb=true,.host_wb_verified=true,.complete_alias_audit=true};
 assert(!trl_backing_release_ready(&f.lease,frozen,&evidence));
}
static void freeze_interleaving_and_release(void) {
 struct fixture f;init(&f,9,SLOTS);f.close_during_init=true;
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 assert(!f.directory.closed&&!f.directory.operations&&f.pool.live==3);
 struct trl_freeze frozen=freeze(&f);
 assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live==3);
 transition(&f,frozen,TRL_UC);clean(&f);
 frozen=freeze(&f);
 struct trl_final_evidence evidence={.freeze=frozen,.serial=999,.parked=3,.vmcs_gone=3,.refills_revoked=3,
  .roots_flushed=3,.caches_flushed=3,.restored_wb=true,.host_wb_verified=true,.complete_alias_audit=true};
 assert(!f.lease.live_tokens&&f.lease.type==TRL_UC);
 assert(!trl_backing_release_ready(&f.lease,frozen,&evidence)); /* zero UC tokens do not free DATA */
 transition(&f,frozen,TRL_WB);frozen=freeze(&f);evidence.freeze=frozen;
 evidence.host_wb_verified=false;assert(!trl_backing_release_ready(&f.lease,frozen,&evidence));
 evidence.host_wb_verified=true;assert(trl_backing_release_ready(&f.lease,frozen,&evidence));
}
static _Alignas(4096) uint64_t walk_tables[4][512];
struct resolver_context {
 struct fixture *fixture;
 struct nested_ept_context_item *other;
 unsigned calls,fail_call;
};
static bool trusted_resolve(void *context,const struct tr_registry_walk *walk,
 uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *permissions) {
 (void)need;struct resolver_context *r=context;struct fixture *f=r->fixture;
 assert(!f->lock_depth);r->calls++;
 metadata_lock(f);
 assert(f->directory.operations==1&&f->bridge.refill_operation&&f->bridge.refill_slot);
 assert(f->bridge.refill_slot->directory.record.refills==1);
 assert(walk->virtual_key==0x1001e&&walk->backing_hpa==DATA&&walk->type==f->lease.type&&
        walk->type_epoch==f->lease.type_epoch);
 assert(!trl_close(&f->lease)&&!f->directory.closed);
 metadata_unlock(f);
 /* A permit for A must never authorize direct fill of another live root. */
 if(r->other) {
  struct nested_ept_result mapped=mapping(walk->type,DATA);
  assert(nested_ept_sparse_fill(&r->other->tree,0xf000,&mapped)==NESTED_SPARSE_INVALID);
 }
 if(r->fail_call==r->calls)return false;
 *permissions=3;*view=NULL;
 if(gpa==0xf000){*hpa=DATA;return true;}
 if(gpa<0x10000||gpa>0x13000||(gpa&4095))return false;
 unsigned n=(gpa-0x10000)/4096;
 *hpa=0x900000+4096*n;*view=walk_tables[n];return true;
}
static void whole_walk_and_locking(void) {
 struct fixture f;init(&f,PAGES,SLOTS);synchronize(&f);
 assert(nested_ept_contexts_select(&f.registry,0x1401e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item *other=f.registry.active;
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 struct tr_registry_slot *slot=active_slot(&f);
 memset(walk_tables,0,sizeof walk_tables);
 for(unsigned i=0;i<3;i++)walk_tables[i][0]=(0x11000+4096*i)|7;
 walk_tables[3][15]=0xf000|0x70|3;
 struct resolver_context resolver={.fixture=&f,.other=other};
 struct nested_ept_policy policy={.maxphys=48};
 assert(typed_root_registry_miss(&f.bridge,f.registry.active,0xf123,3,trusted_resolve,&resolver,&policy)==NESTED_SPARSE_OK);
 assert(resolver.calls==5&&!f.lock_depth&&f.lock_calls&&!slot->directory.record.refills&&!f.directory.operations);
 unsigned allocations=f.allocations;resolver.calls=0;
 assert(typed_root_registry_miss(&f.bridge,f.registry.active,0xf000,1,trusted_resolve,&resolver,&policy)==NESTED_SPARSE_INVALID);
 assert(!resolver.calls&&f.allocations==allocations&&!f.directory.operations&&!slot->directory.record.refills);
 struct nested_ept_result mapped=mapping(TRL_WB,DATA);
 assert(nested_ept_sparse_fill(&f.registry.active->tree,0xe000,&mapped)==NESTED_SPARSE_INVALID);
 assert(nested_ept_contexts_invalidate(&f.registry,2,0)==NESTED_CONTEXT_OK);
 resolver.fail_call=3;
 assert(typed_root_registry_miss(&f.bridge,f.registry.active,0xf000,1,trusted_resolve,&resolver,&policy)==NESTED_SPARSE_INVALID);
 assert(resolver.calls==3&&!f.directory.operations&&!slot->directory.record.refills&&!f.bridge.refill_slot);
 resolver.fail_call=0;resolver.calls=0;f.fail_alloc=true;
 assert(typed_root_registry_miss(&f.bridge,f.registry.active,0xf000,1,trusted_resolve,&resolver,&policy)==NESTED_SPARSE_OOM);
 assert(!f.directory.operations&&!slot->directory.record.refills&&!f.lock_depth);
 f.fail_alloc=false;
 /* The provider can collect evidence unlocked. If somebody changes shared
  * metadata before its return, retirement retains the root and typed token. */
 f.change_version=true;
 assert(!nested_ept_contexts_destroy(&f.registry)&&f.pool.live&&f.lease.live_tokens);
 assert(!f.lock_depth&&!f.directory.operations);clean(&f);
}
int main(void) {
 whole_walk_and_locking();eviction_and_refill();allocation_failures();retirement_failures();committed_retirement_failures();freeze_interleaving_and_release();
 puts("typed_root_registry PASS: actual registry+owner pool, budget 2, dynamic UC retirement; mock physical flush only, no hardware proof");
 return 0;
}
