/* Deterministic pause/resume schedules execute the actual registry/bridge/
 * private pool. Flushes are mocks; no VMX, threads, or hardware evidence. */
#include "typed_root_registry.h"
#include "nested_owner_pool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PAGES 48u
#define SLOTS 16u
#define DATA UINT64_C(0x800000)
static _Alignas(4096) unsigned char memory[2][PAGES][4096];
enum fault {
 OK,NO_EVIDENCE,ZERO_SERIAL,BAD_ACK,STALE_SCOPE,
 BAD_DOMAIN,BAD_INCARNATION,BAD_ROOT_ID,BAD_ROOT_INCARNATION,BAD_LIFETIME,BAD_OWNER,
 BAD_EPTP,BAD_HPA,BAD_GENERATION,BAD_HISTORY,BAD_EPOCH,BAD_RETIRE_NONCE,BAD_OPERATION,
 CHANGED_ID,CHANGED_EPTP,CHANGED_HPA,CHANGED_GENERATION,CHANGED_HISTORY,CHANGED_EPOCH,
 CHANGED_ITEM_EPTP,CHANGED_ITEM_GENERATION,CHANGED_ITEM_PINS,CHANGED_RECORD_PINS,
 RESTART_RETIREMENT,END_OPERATION,REUSE_OPERATION,POISON,EXHAUST_VERSION
};
struct fixture;
struct owner {
 struct fixture *f;
 unsigned cpu,reserved,allocations,frees,flushes,evidences;
 uint64_t serial,last_eptp;
 bool interleave,fail_flush;
 enum fault fault;
 uint64_t remote_acked;
 struct tr_registry_retire_scope previous_scope;
 struct typed_root_registry bridge;
 struct nested_ept_contexts registry;
 struct nested_owner_pool pool;
 struct nested_sparse_page pages[PAGES];
 struct nested_owner_slot pool_slots[PAGES];
 struct tr_registry_slot slots[SLOTS];
};
struct fixture {
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct owner owners[2];
 unsigned depth,locks,peer_steps;
 bool root_scoped;
};
static void lock(void *p) {struct fixture *f=p;assert(!f->depth);f->depth=1;f->locks++;}
static void unlock(void *p) {struct fixture *f=p;assert(f->depth==1);f->depth=0;}
static bool alloc(void *p,struct nested_sparse_page *out) {
 struct owner *o=p;assert(!o->f->depth);o->allocations++;
 assert(o->bridge.directory.busy||o->bridge.refill_operation);
 return nested_owner_pool_alloc(&o->pool,out);
}
static void release(void *p,struct nested_sparse_page page) {
 struct owner *o=p;assert(!o->f->depth);
 for(unsigned i=0;i<o->reserved;i++) {
  struct tr_registry_slot *s=&o->slots[i];
  if(s->directory.record.attached&&s->directory.record.hpa==page.hpa) {
   assert(s->directory.forgotten&&s->directory.receipt.live);
   assert(s->typed.state==TRL_RETIRED&&!s->typed.token_live);
   assert(s->typed.receipt_version==s->receipt_version&&s->receipt_version);
  }
 }
 o->frees++;nested_owner_pool_free(&o->pool,page);assert(!o->pool.fault);
}
static bool flush(void *p,uint64_t eptp) {
 struct owner *o=p;assert(!o->f->depth&&(eptp&4095)==0x1e);o->flushes++;
 if(o->fail_flush)return false;
 o->last_eptp=eptp;o->serial++;return true;
}
static bool reserve(void *p,struct tr_registry_slot **out,uint64_t *life) {
 struct owner *o=p;assert(o->f->depth==1);
 if(o->reserved==SLOTS)return false;
 *out=&o->slots[o->reserved++];*life=100+o->cpu*SLOTS+o->reserved;return true;
}
static struct tr_registry_slot *active(struct owner *o) {
 for(unsigned i=0;i<o->reserved;i++)if(&o->slots[i].directory==o->registry.active->transaction_token)return &o->slots[i];
 assert(false);return NULL;
}
static void resident(struct owner *o,unsigned cpu) {
 struct fixture *f=o->f;struct tr_registry_slot *s=active(o);struct td_operation op;uint64_t n;
 lock(f);assert(td_begin(&f->directory,&op,&n));
 assert(td_resident(&f->directory,&op,n,&s->directory.record,s->directory.identity,cpu));
 assert(td_end(&f->directory,&op,n));unlock(f);
}
static struct nested_ept_result mapping(unsigned type) {
 return (struct nested_ept_result){.status=NESTED_EPT_OK,.l1_gpa=0xf000,.hpa=DATA,
  .effective12=3,.effective02=3,.leaf_resolved=true,.memory_explicit=true,
  .leaf_type=type,.walk_type=TRL_WB,.ignore_pat=true};
}
static void peer_work(struct owner *a) {
 struct fixture *f=a->f;struct owner *b=&f->owners[1];assert(a->cpu==0&&!f->depth);
 lock(f);assert(f->directory.operations==1&&a->bridge.directory.busy);
 assert(!trl_close(&f->lease)&&!f->directory.closed);unlock(f);
 assert(nested_ept_contexts_select(&b->registry,0x2001e)==NESTED_CONTEXT_OK);f->peer_steps++;
 struct nested_ept_context_item *p=nested_ept_contexts_pin_active(&b->registry);
 assert(p&&active(b)->directory.record.pins==1);f->peer_steps++;
 struct nested_ept_result m=mapping(f->lease.type),out;
 assert(typed_root_registry_fill(&b->bridge,p,0xf000,&m)==NESTED_SPARSE_OK);
 assert(typed_root_registry_lookup(&b->bridge,p,0xf000,3,&out)==NESTED_SPARSE_HIT);f->peer_steps++;
 resident(b,1);f->peer_steps++;
 assert(nested_ept_contexts_unpin(&b->registry,p));
 assert(nested_ept_contexts_destroy(&b->registry));f->peer_steps++;
 assert(!b->pool.live&&b->slots[0].typed.state==TRL_RETIRED);
 lock(f);assert(f->directory.operations==1&&!trl_close(&f->lease));unlock(f);
}
static bool evidence(void *p,struct tr_registry_slot *slot,struct td_retire_ticket t,
 uint64_t version,struct tr_registry_flush_evidence *out) {
 struct owner *o=p;assert(!o->f->depth&&o->last_eptp==t.full_eptp&&o->serial);o->evidences++;
 lock(o->f);assert(slot->directory.item&&slot->directory.record.state==TD_RETIRING);
 assert(slot->typed.token_live&&!slot->directory.receipt.live);unlock(o->f);
 if(o->interleave){o->interleave=false;peer_work(o);}
 *out=(struct tr_registry_flush_evidence){t,version,t.history,o->serial};return true;
}
static bool root_evidence(void *p,struct tr_registry_slot *slot,
 struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out) {
 struct owner *o=p;struct fixture *f=o->f;assert(!f->depth&&o->last_eptp==scope.ticket.full_eptp&&o->serial);
 o->evidences++;lock(f);
 assert(slot->directory.item&&slot->directory.record.state==TD_RETIRING);
 assert(!slot->directory.receipt.live&&slot->typed.token_live);
 assert(f->directory.operations==(o->cpu==1&&f->owners[0].bridge.directory.busy?2u:1u));
 assert(!trl_close(&f->lease)&&!td_close(&f->directory)&&!f->directory.closed);
 struct trl_freeze frozen;assert(!trl_seal(&f->lease,&frozen));unlock(f);
 if(o->interleave){o->interleave=false;peer_work(o);}
 *out=(struct tr_registry_root_evidence){.scope=scope,
  .acked=scope.ticket.history&((UINT64_C(1)<<o->cpu)|o->remote_acked),.flush_serial=o->serial};
 struct td_record *r=&slot->directory.record;
 lock(f);
 switch(o->fault) {
 case OK:break;
 case NO_EVIDENCE:unlock(f);return false;
 case ZERO_SERIAL:out->flush_serial=0;break;
 case BAD_ACK:out->acked^=1;break;
 case STALE_SCOPE:out->scope=o->previous_scope;break;
 case BAD_DOMAIN:out->scope.ticket.identity.domain++;break;
 case BAD_INCARNATION:out->scope.ticket.identity.incarnation++;break;
 case BAD_ROOT_ID:out->scope.ticket.identity.root_id++;break;
 case BAD_ROOT_INCARNATION:out->scope.ticket.identity.root_incarnation++;break;
 case BAD_LIFETIME:out->scope.ticket.identity.allocator_lifetime++;break;
 case BAD_OWNER:out->scope.ticket.identity.owner^=1;break;
 case BAD_EPTP:out->scope.ticket.full_eptp^=1;break;
 case BAD_HPA:out->scope.ticket.hpa+=4096;break;
 case BAD_GENERATION:out->scope.ticket.cache_generation++;break;
 case BAD_HISTORY:out->scope.ticket.history^=2;break;
 case BAD_EPOCH:out->scope.ticket.type_epoch++;break;
 case BAD_RETIRE_NONCE:out->scope.ticket.nonce++;break;
 case BAD_OPERATION:out->scope.operation_nonce++;break;
 case CHANGED_ID:r->identity.allocator_lifetime++;break;
 case CHANGED_EPTP:r->full_eptp^=1;break;
 case CHANGED_HPA:r->hpa+=4096;break;
 case CHANGED_GENERATION:r->cache_generation++;break;
 case CHANGED_HISTORY:r->history^=2;break;
 case CHANGED_EPOCH:f->directory.type_epoch++;f->lease.type_epoch++;break;
 case CHANGED_ITEM_EPTP:slot->directory.item->tree.eptp^=1;break;
 case CHANGED_ITEM_GENERATION:slot->directory.item->tree.generation++;break;
 case CHANGED_ITEM_PINS:slot->directory.item->pins++;break;
 case CHANGED_RECORD_PINS:r->pins++;break;
 case RESTART_RETIREMENT: {
  struct td_operation op;uint64_t n;struct td_retire_ticket fresh;
  assert(td_begin(&f->directory,&op,&n));
  assert(td_retain(&f->directory,&op,n,r,slot->directory.identity));
  assert(td_retire_begin(&f->directory,&op,n,r,slot->directory.identity,&fresh));
  assert(fresh.nonce!=scope.ticket.nonce);assert(td_end(&f->directory,&op,n));break;
 }
 case END_OPERATION:case REUSE_OPERATION:
  assert(td_end(&f->directory,&o->bridge.directory.operation,o->bridge.directory.nonce));
  if(o->fault==REUSE_OPERATION)assert(td_begin(&f->directory,&o->bridge.directory.operation,&o->bridge.directory.nonce));
  break;
 case POISON:trl_poison(&f->lease);break;
 case EXHAUST_VERSION:f->directory.version=UINT64_MAX;break;
 }
 o->previous_scope=scope;unlock(f);return true;
}
static void init(struct fixture *f,bool root_scoped) {
 memset(f,0,sizeof *f);f->root_scoped=root_scoped;assert(td_init(&f->directory,10,11,3,1));
 assert(trl_init(&f->lease,&f->directory,(struct trl_backing){DATA,99,true},2*SLOTS));
 for(unsigned cpu=0;cpu<2;cpu++) {
  struct owner *o=&f->owners[cpu];o->f=f;o->cpu=cpu;
  for(unsigned i=0;i<PAGES;i++)o->pages[i]=(struct nested_sparse_page){0x100000+cpu*0x100000+4096*i,memory[cpu][i]};
  assert(nested_owner_pool_init(&o->pool,cpu,1,o->pages,o->pool_slots,PAGES,48));
  const struct tr_registry_provider provider={reserve,evidence};
  const struct tr_registry_root_provider root_provider={reserve,root_evidence};
  assert(root_scoped?typed_root_registry_init_root_scoped(&o->bridge,&f->lease,cpu,DATA,&root_provider,o):
                    typed_root_registry_init(&o->bridge,&f->lease,cpu,DATA,&provider,o));
  assert(typed_root_registry_set_sync(&o->bridge,lock,unlock,f));
  const struct nested_sparse_ops ops={alloc,release,flush};
  assert(nested_ept_contexts_init_budget(&o->registry,&ops,o,48,2,true));
  assert(nested_ept_contexts_set_transactions(&o->registry,typed_root_registry_transactions(),&o->bridge));
 }
}
static void legacy_witness(void) {
 struct fixture f;init(&f,false);struct owner *a=&f.owners[0];
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);resident(a,0);
 a->interleave=true;
 assert(!nested_ept_contexts_destroy(&a->registry));
 assert(f.peer_steps==5&&!f.depth&&!f.directory.operations&&!f.directory.poisoned);
 assert(a->pool.live==3&&!a->frees&&a->slots[0].typed.token_live&&!a->slots[0].directory.receipt.live);
 assert(a->slots[0].directory.record.state==TD_RETIRING);
 assert(nested_ept_contexts_destroy(&a->registry)&&!a->pool.live&&!f.lease.live_tokens);
 puts("PASS legacy witness: valid peer create/pin/refill/residency/retire rejects paused owner retirement, retained safely");
}
static void transition(struct fixture *f,unsigned type) {
 lock(f);struct trl_freeze frozen;assert(trl_close(&f->lease)&&trl_seal(&f->lease,&frozen));
 uint64_t epoch=f->lease.type_epoch+1;
 for(struct trl_binding *b=f->lease.bindings;b;b=b->next)if(b->token_live) {
  struct trl_token token,prepared;assert(trl_token_read(&f->lease,b,&token));
  assert(trl_prepare_token(&f->lease,frozen,b,token,epoch,type,&prepared));
 }
 struct trl_transition_evidence e={.freeze=frozen,.new_epoch=epoch,.serial=100+epoch,
  .closed=3,.drained=3,.tlb_before=3,.cache_before=3,.tlb_after=3,.cache_after=3,.destination=type,
  .complete_alias_audit=true,.all_refills_revoked=true,.all_roots_invalidated=true,
  .host_break_before_make=true,.all_aliases_retyped=true,.owner_stamps_ready=true};
 assert(trl_reopen(&f->lease,frozen,&e));unlock(f);
}
static void concurrent_success(unsigned type) {
 struct fixture f;init(&f,true);struct owner *a=&f.owners[0],*b=&f.owners[1];
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);resident(a,0);
 if(type==TRL_UC)transition(&f,TRL_UC);
 a->interleave=true;
 assert(nested_ept_contexts_destroy(&a->registry));
 assert(f.peer_steps==5&&!f.depth&&!f.directory.operations&&!f.directory.poisoned);
 assert(!a->pool.live&&!b->pool.live&&!f.lease.live_tokens);
 struct tr_registry_slot *as=&a->slots[0],*bs=&b->slots[0];
 assert(as->receipt_version>as->evidence_version+1);
 assert(bs->receipt_version>as->evidence_version&&bs->receipt_version<as->receipt_version);
 assert(as->directory.receipt.commit_version==as->receipt_version);
 assert(as->directory.receipt.retirement_nonce==as->evidence_ticket.nonce);
 assert(as->directory.receipt.type_epoch==f.lease.type_epoch);
 struct trl_freeze frozen;lock(&f);assert(trl_close(&f.lease)&&trl_seal(&f.lease,&frozen));
 struct trl_snapshot snapshots[2];size_t n=99;
 assert(trl_snapshot_read(&f.lease,frozen,snapshots,2,&n)&&n==2);
 for(size_t i=0;i<n;i++)assert(snapshots[i].root.receipt&&!snapshots[i].scan_allowed);
 struct trl_final_evidence e={.freeze=frozen,.serial=90,.parked=3,.vmcs_gone=3,.refills_revoked=3,
  .roots_flushed=3,.caches_flushed=3,.restored_wb=true,.host_wb_verified=true,.complete_alias_audit=true};
 if(type==TRL_UC)assert(!trl_backing_release_ready(&f.lease,frozen,&e));
 e.parked=1;assert(!trl_backing_release_ready(&f.lease,frozen,&e));unlock(&f);
}
static void concurrent_eviction(unsigned type) {
 struct fixture f;init(&f,true);struct owner *a=&f.owners[0];
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);resident(a,0);
 struct tr_registry_slot *old=active(a);
 assert(nested_ept_contexts_select(&a->registry,0x1101e)==NESTED_CONTEXT_OK);
 if(type==TRL_UC)transition(&f,TRL_UC);
 a->interleave=true;
 assert(nested_ept_contexts_select(&a->registry,0x1201e)==NESTED_CONTEXT_OK);
 assert(f.peer_steps==5&&a->registry.count==2&&a->registry.active->key==0x1201e);
 assert(a->pool.live==6&&a->frees==3&&!a->registry.poisoned&&!f.lease.poisoned);
 assert(old->directory.forgotten&&old->typed.state==TRL_RETIRED&&old->directory.receipt.live);
 assert(old->receipt_version>old->evidence_version+1);
 assert(active(a)->typed.state==TRL_LIVE&&active(a)->directory.record.state==TD_LIVE);
 assert(f.lease.live_tokens==2&&!f.directory.operations&&!f.depth);
 assert(nested_ept_contexts_destroy(&a->registry)&&!a->pool.live&&!f.lease.live_tokens);
}
static void negative_callbacks(void) {
 for(enum fault fault=NO_EVIDENCE;fault<=EXHAUST_VERSION;fault++) {
  struct fixture f;init(&f,true);struct owner *a=&f.owners[0];
  assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);resident(a,0);
  a->fault=fault;assert(!nested_ept_contexts_destroy(&a->registry));
  assert(a->pool.live==3&&!a->frees&&a->slots[0].typed.token_live);
  assert(!a->slots[0].directory.receipt.live&&!a->slots[0].directory.forgotten&&!f.depth);
  lock(&f);assert(!trl_close(&f.lease));unlock(&f);
 }
}
static void retry_and_history(void) {
 struct fixture f;init(&f,true);struct owner *a=&f.owners[0];
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);resident(a,0);resident(a,1);
 assert(!nested_ept_contexts_destroy(&a->registry)); /* local flush cannot ACK remote CPU */
 struct tr_registry_retire_scope first=a->previous_scope;
 a->remote_acked=2;a->fault=STALE_SCOPE;
 assert(!nested_ept_contexts_destroy(&a->registry)); /* same retirement, previous operation */
 assert(first.ticket.nonce==a->previous_scope.ticket.nonce&&first.operation_nonce!=a->previous_scope.operation_nonce);
 a->fault=OK;assert(nested_ept_contexts_destroy(&a->registry));
 assert(a->slots[0].directory.receipt.acked==3&&!a->pool.live);
 init(&f,true);a=&f.owners[0];
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);
 a->fail_flush=true;assert(!nested_ept_contexts_destroy(&a->registry));
 assert(!a->evidences&&!a->frees&&a->pool.live==3);
 a->fail_flush=false;assert(nested_ept_contexts_destroy(&a->registry));
}
static void reuse_and_budget(void) {
 struct fixture f;init(&f,true);struct owner *a=&f.owners[0];
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);
 struct tr_registry_slot *old=active(a);uint64_t hpa=old->directory.record.hpa;
 assert(nested_ept_contexts_destroy(&a->registry));
 const struct nested_sparse_ops ops={alloc,release,flush};
 assert(nested_ept_contexts_init_budget(&a->registry,&ops,a,48,2,true));
 assert(nested_ept_contexts_set_transactions(&a->registry,typed_root_registry_transactions(),&a->bridge));
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);
 assert(active(a)->directory.record.hpa==hpa&&active(a)->directory.identity.root_id!=old->directory.identity.root_id);
 assert(active(a)->directory.identity.allocator_lifetime!=old->directory.identity.allocator_lifetime);
 a->fault=STALE_SCOPE;assert(!nested_ept_contexts_destroy(&a->registry));
 assert(a->pool.live==3&&a->slots[1].typed.token_live&&!a->slots[1].directory.receipt.live);
 a->fault=OK;assert(nested_ept_contexts_destroy(&a->registry));
 init(&f,true);a=&f.owners[0];f.lease.budget=1;
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);
 unsigned allocations=a->allocations;
 assert(nested_ept_contexts_select(&a->registry,0x1101e)==NESTED_CONTEXT_OOM);
 assert(a->allocations==allocations&&a->reserved==1&&!a->frees);assert(nested_ept_contexts_destroy(&a->registry));
 init(&f,true);a=&f.owners[0];
 assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item *p=nested_ept_contexts_pin_active(&a->registry);assert(p);
 assert(!nested_ept_contexts_destroy(&a->registry)&&!a->evidences&&!a->frees);
 assert(nested_ept_contexts_unpin(&a->registry,p));assert(nested_ept_contexts_destroy(&a->registry));
}
static void durable_receipt_negatives(void) {
 for(unsigned field=0;field<6;field++) {
  struct fixture f;init(&f,true);struct owner *a=&f.owners[0];
  assert(nested_ept_contexts_select(&a->registry,0x1001e)==NESTED_CONTEXT_OK);
  uint64_t version=f.lease.version;f.lease.version=UINT64_MAX;
  assert(!nested_ept_contexts_destroy(&a->registry));
  struct tr_registry_slot *s=&a->slots[0];struct td_receipt saved=s->directory.receipt;
  assert(saved.live&&s->directory.record.state==TD_RETIRED&&s->typed.token_live&&!a->frees);
  unsigned callbacks=a->evidences;f.lease.version=version;
  switch(field) {
  case 0:s->directory.receipt.retirement_nonce++;break;
  case 1:s->directory.receipt.type_epoch++;break;
  case 2:s->directory.receipt.commit_version--;break;
  case 3:s->directory.receipt.cache_generation++;break;
  case 4:s->directory.receipt.identity.allocator_lifetime++;break;
  case 5:s->directory.receipt.flush_serial++;break;
  }
  assert(!nested_ept_contexts_destroy(&a->registry));
  assert(a->pool.live==3&&!a->frees&&a->evidences==callbacks&&s->typed.token_live);
  s->directory.receipt=saved;assert(nested_ept_contexts_destroy(&a->registry));
 }
}
int main(int argc,char **argv) {
 if(argc==2&&!strcmp(argv[1],"--interleaving-only")){concurrent_success(TRL_WB);return 0;}
 legacy_witness();concurrent_success(TRL_WB);concurrent_success(TRL_UC);
 concurrent_eviction(TRL_WB);concurrent_eviction(TRL_UC);
 negative_callbacks();retry_and_history();reuse_and_budget();durable_receipt_negatives();
 puts("PASS N39A root-scoped actual registry interleavings: peer commit reorder, WB/UC, all identity fields, stale attempts/receipts, HPA reuse, ACK/flush/pin/poison/budget retention; mock flush only");
 return 0;
}
