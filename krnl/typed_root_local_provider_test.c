/* Host execution of the production N39B1 provider and actual bridge/registry.
 * Only the physical CPU/INVEPT and metadata lock are injected. No VMX runs. */
#include "typed_root_local_provider.h"
#include "nested_owner_pool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PAGES 48u
#define SLOTS 16u
#define DATA UINT64_C(0x800000)
static _Alignas(4096) unsigned char memory[2][PAGES][4096];
enum mutation {
 NONE, DOMAIN, INCARNATION, ROOT_ID, ROOT_INCARNATION, LIFETIME, OWNER,
 EPTP, HPA, GENERATION, HISTORY, EPOCH, RETIRE_NONCE, OP_NONCE,
 ITEM_EPTP, ITEM_GENERATION, ITEM_KEY, ITEM_TOKEN, ITEM_PINS, RECORD_PINS, REFILLS,
 SLOT_BRIDGE, DIRECTORY_BRIDGE, UNLINK_SLOT, UNLINK_DIRECTORY_SLOT, FORGOTTEN,
 TOKEN_EPOCH, TOKEN_TYPE, LEASE_POISON, DIRECTORY_POISON, BRIDGE_FAULT,
 END_OPERATION, REUSE_OPERATION, RESTART_RETIREMENT, SERIAL_EXHAUST, VERSION_EXHAUST,
 LEASE_VERSION_EXHAUST, PHYSICAL_CPU, HEALTH_FAULT, CLOSED, LAST_MUTATION
};
enum injection { CLEAN, INVEPT_FAIL, INVEPT_FALSE, WRONG_EPTP, WRONG_CPU, WRONG_ACK,
 OUT_ACK, OUT_SERIAL, OUT_SCOPE, OUT_NONCE, LOCK_PRE, LOCK_POST };
struct fixture;
struct owner {
 struct fixture *f;
 unsigned cpu,reserved,allocations,frees,flushes,invepts,provider_calls,provider_locks;
 bool interleave,mutate_before,corrupt_scope;
 enum mutation mutation;
 enum injection injection;
 struct tr_registry_slot *pending_slot;
 struct tr_registry_retire_scope pending_scope;
 struct typed_root_registry bridge;
 struct nested_ept_contexts registry;
 struct nested_owner_pool pool;
 struct nested_sparse_page pages[PAGES];
 struct nested_owner_slot pool_slots[PAGES];
 struct tr_registry_slot slots[SLOTS];
 struct tr_local_completed completed[SLOTS];
 struct tr_local_provider provider;
};
struct fixture {
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct owner owners[2];
 unsigned depth,cpu,peer_steps;
 uint64_t serial;
 bool fault;
};
static void lock(void *p){struct fixture *f=p;assert(!f->depth);f->depth=1;}
static void unlock(void *p){struct fixture *f=p;assert(f->depth==1);f->depth=0;}
static bool provider_lock(void *p){
 struct owner *o=p;unsigned call=++o->provider_locks;
 assert(!o->f->depth);
 if((o->injection==LOCK_PRE&&call==1)||(o->injection==LOCK_POST&&call==2))return false;
 lock(o->f);return true;
}
static void provider_unlock(void *p){struct owner *o=p;unlock(o->f);}
static unsigned current_cpu(void *p){struct owner *o=p;assert(o->f->depth==1);return o->f->cpu;}
static bool healthy(void *p){struct owner *o=p;assert(o->f->depth==1);return !o->f->fault;}
static bool alloc(void *p,struct nested_sparse_page *out){
 struct owner *o=p;assert(!o->f->depth);o->allocations++;
 assert(o->bridge.directory.busy||o->bridge.refill_operation);
 return nested_owner_pool_alloc(&o->pool,out);
}
static void release(void *p,struct nested_sparse_page page){
 struct owner *o=p;assert(!o->f->depth);
 for(unsigned i=0;i<o->reserved;i++)if(o->slots[i].directory.record.hpa==page.hpa){
  lock(o->f);assert(tr_local_completed_matches(&o->provider,&o->slots[i]));unlock(o->f);
 }
 o->frees++;nested_owner_pool_free(&o->pool,page);assert(!o->pool.fault);
}
static bool flush(void *p,uint64_t eptp){
 struct owner *o=p;assert(!o->f->depth&&(eptp&4095)==0x1e);o->flushes++;return true;
}
static bool reserve(void *p,struct tr_registry_slot **out,uint64_t *life){
 struct owner *o=p;assert(o->f->depth==1);if(o->reserved==SLOTS)return false;
 *out=&o->slots[o->reserved++];*life=100+o->cpu*SLOTS+o->reserved;return true;
}
static struct tr_registry_slot *active(struct owner *o){
 for(unsigned i=0;i<o->reserved;i++)if(&o->slots[i].directory==o->registry.active->transaction_token)return &o->slots[i];
 assert(false);return NULL;
}
static void resident(struct owner *o,unsigned cpu){
 struct fixture *f=o->f;struct tr_registry_slot *s=active(o);struct td_operation op;uint64_t n;
 lock(f);assert(td_begin(&f->directory,&op,&n));
 assert(td_resident(&f->directory,&op,n,&s->directory.record,s->directory.identity,cpu));
 assert(td_end(&f->directory,&op,n));unlock(f);
}
static void alter_scope(struct tr_registry_retire_scope *s,enum mutation m){
 switch(m){
 case DOMAIN:s->ticket.identity.domain++;break;
 case INCARNATION:s->ticket.identity.incarnation++;break;
 case ROOT_ID:s->ticket.identity.root_id++;break;
 case ROOT_INCARNATION:s->ticket.identity.root_incarnation++;break;
 case LIFETIME:s->ticket.identity.allocator_lifetime++;break;
 case OWNER:s->ticket.identity.owner^=1;break;
 case EPTP:s->ticket.full_eptp^=1;break;
 case HPA:s->ticket.hpa+=4096;break;
 case GENERATION:s->ticket.cache_generation++;break;
 case HISTORY:s->ticket.history^=3;break;
 case EPOCH:s->ticket.type_epoch++;break;
 case RETIRE_NONCE:s->ticket.nonce++;break;
 case OP_NONCE:s->operation_nonce++;break;
 default:assert(false);
 }
}
static void mutate(struct owner *o){
 struct fixture *f=o->f;struct tr_registry_slot *s=o->pending_slot;struct td_record *r=&s->directory.record;
 struct td_registry_bridge *b=&o->bridge.directory;assert(f->depth==1);
 switch(o->mutation){
 case NONE:break;
 case DOMAIN:r->identity.domain++;break;
 case INCARNATION:r->identity.incarnation++;break;
 case ROOT_ID:r->identity.root_id++;break;
 case ROOT_INCARNATION:r->identity.root_incarnation++;break;
 case LIFETIME:r->identity.allocator_lifetime++;break;
 case OWNER:r->identity.owner^=1;break;
 case EPTP:r->full_eptp^=1;break;
 case HPA:r->hpa+=4096;break;
 case GENERATION:r->cache_generation++;break;
 case HISTORY:r->history^=3;break;
 case EPOCH:r->retirement_epoch++;break;
 case RETIRE_NONCE:r->retirement_nonce++;break;
 case OP_NONCE:b->nonce++;break;
 case ITEM_EPTP:s->directory.item->tree.eptp^=1;break;
 case ITEM_GENERATION:s->directory.item->tree.generation++;break;
 case ITEM_KEY:s->directory.item->key+=4096;break;
 case ITEM_TOKEN:s->directory.item->transaction_token=NULL;break;
 case ITEM_PINS:s->directory.item->pins++;break;
 case RECORD_PINS:r->pins++;break;
 case REFILLS:r->refills++;break;
 case SLOT_BRIDGE:s->bridge=&f->owners[o->cpu^1].bridge;break;
 case DIRECTORY_BRIDGE:s->directory.bridge=&f->owners[o->cpu^1].bridge.directory;break;
 case UNLINK_SLOT:o->bridge.slots=NULL;break;
 case UNLINK_DIRECTORY_SLOT:b->slots=NULL;break;
 case FORGOTTEN:s->directory.forgotten=true;break;
 case TOKEN_EPOCH:s->typed.token.type_epoch++;break;
 case TOKEN_TYPE:s->typed.token.type^=6;break;
 case LEASE_POISON:trl_poison(&f->lease);break;
 case DIRECTORY_POISON:td_poison(&f->directory);break;
 case BRIDGE_FAULT:b->faulted=true;break;
 case END_OPERATION:case REUSE_OPERATION:
  assert(td_end(&f->directory,&b->operation,b->nonce));
  if(o->mutation==REUSE_OPERATION)assert(td_begin(&f->directory,&b->operation,&b->nonce));
  break;
 case RESTART_RETIREMENT:{
  struct td_operation op;uint64_t n;struct td_retire_ticket fresh;
  assert(td_begin(&f->directory,&op,&n));
  assert(td_retain(&f->directory,&op,n,r,s->directory.identity));
  assert(td_retire_begin(&f->directory,&op,n,r,s->directory.identity,&fresh));
  assert(fresh.nonce!=o->pending_scope.ticket.nonce);assert(td_end(&f->directory,&op,n));break;
 }
 case SERIAL_EXHAUST:f->serial=UINT64_MAX;break;
 case VERSION_EXHAUST:f->directory.version=UINT64_MAX;break;
 case LEASE_VERSION_EXHAUST:f->lease.version=UINT64_MAX;break;
 case PHYSICAL_CPU:f->cpu^=1;break;
 case HEALTH_FAULT:f->fault=true;break;
 case CLOSED:f->directory.closed=true;break;
 default:assert(false);
 }
}
static void peer_work(struct owner *a){
 struct fixture *f=a->f;struct owner *b=&f->owners[1];assert(a->cpu==0&&!f->depth);
 lock(f);assert(f->directory.operations==1&&!trl_close(&f->lease));unlock(f);
 f->cpu=1;
 assert(nested_ept_contexts_select(&b->registry,0x2001e)==NESTED_CONTEXT_OK);f->peer_steps++;
 struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&b->registry);assert(item);f->peer_steps++;
 struct nested_ept_result m={.status=NESTED_EPT_OK,.l1_gpa=0xf000,.hpa=DATA,
  .effective12=3,.effective02=3,.leaf_resolved=true,.memory_explicit=true,
  .leaf_type=f->lease.type,.walk_type=TRL_WB,.ignore_pat=true},out;
 assert(typed_root_registry_fill(&b->bridge,item,0xf000,&m)==NESTED_SPARSE_OK);
 assert(typed_root_registry_lookup(&b->bridge,item,0xf000,3,&out)==NESTED_SPARSE_HIT);f->peer_steps++;
 resident(b,1);f->peer_steps++;
 assert(nested_ept_contexts_unpin(&b->registry,item)&&nested_ept_contexts_destroy(&b->registry));f->peer_steps++;
 assert(!b->pool.live);f->cpu=0;
}
static bool invalidate(void *p,uint64_t eptp,struct tr_local_invalidation *out){
 struct owner *o=p;struct fixture *f=o->f;assert(!f->depth&&eptp==o->pending_scope.ticket.full_eptp);
 assert(eptp!=o->pending_slot->directory.virtual_key);o->invepts++;
 lock(f);assert(td_retire_pending(&f->directory,&o->bridge.directory.operation,
  o->pending_scope.operation_nonce,&o->pending_slot->directory.record,
  o->pending_slot->directory.identity,o->pending_scope.ticket));
 assert(!trl_close(&f->lease)&&!td_close(&f->directory));unlock(f);
 if(o->interleave){o->interleave=false;peer_work(o);}
 if(!o->mutate_before){lock(f);mutate(o);unlock(f);}
 *out=(struct tr_local_invalidation){.full_eptp=eptp,.acked=UINT64_C(1)<<o->cpu,.cpu=o->cpu,.success=true};
 switch(o->injection){
 case INVEPT_FAIL:return false;
 case INVEPT_FALSE:out->success=false;break;
 case WRONG_EPTP:out->full_eptp^=1;break;
 case WRONG_CPU:out->cpu^=1;break;
 case WRONG_ACK:out->acked^=3;break;
 default:break;
 }
 return true;
}
static bool evidence(void *p,struct tr_registry_slot *slot,
 struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out){
 struct owner *o=p;assert(!o->f->depth);o->provider_calls++;o->provider_locks=0;
 o->pending_slot=slot;o->pending_scope=scope;
 if(o->mutate_before){lock(o->f);mutate(o);unlock(o->f);}
 if(o->corrupt_scope)alter_scope(&scope,o->mutation);
 struct tr_registry_root_evidence supplied={0};
 bool ok=tr_local_provider_flush(&o->provider,slot,scope,&supplied);
 if(!ok){assert(!supplied.flush_serial&&!supplied.scope.operation_nonce);return false;}
 *out=supplied;
 switch(o->injection){
 case OUT_ACK:out->acked^=3;break;
 case OUT_SERIAL:out->flush_serial=0;break;
 case OUT_SCOPE:out->scope.ticket.full_eptp^=1;break;
 case OUT_NONCE:out->scope.operation_nonce++;break;
 default:break;
 }
 return true;
}
static void init(struct fixture *f){
 memset(f,0,sizeof *f);assert(td_init(&f->directory,10,11,3,1));
 assert(trl_init(&f->lease,&f->directory,(struct trl_backing){DATA,99,true},2*SLOTS));
 for(unsigned cpu=0;cpu<2;cpu++){
  struct owner *o=&f->owners[cpu];o->f=f;o->cpu=cpu;
  for(unsigned i=0;i<PAGES;i++)o->pages[i]=(struct nested_sparse_page){0x100000+cpu*0x100000+4096*i,memory[cpu][i]};
  assert(nested_owner_pool_init(&o->pool,cpu,1,o->pages,o->pool_slots,PAGES,48));
  const struct tr_registry_root_provider provider={reserve,evidence};
  assert(typed_root_registry_init_root_scoped(&o->bridge,&f->lease,cpu,DATA,&provider,o));
  assert(typed_root_registry_set_sync(&o->bridge,lock,unlock,f));
  o->provider=(struct tr_local_provider){.bridge=&o->bridge,.slots=o->slots,.completed=o->completed,
   .slot_count=SLOTS,.owner=cpu,.serial=&f->serial,.lock=provider_lock,.unlock=provider_unlock,
   .current_cpu=current_cpu,.healthy=healthy,.invalidate=invalidate,.context=o};
  const struct nested_sparse_ops ops={alloc,release,flush};
  assert(nested_ept_contexts_init_budget(&o->registry,&ops,o,48,2,true));
  assert(nested_ept_contexts_set_transactions(&o->registry,typed_root_registry_transactions(),&o->bridge));
 }
}
static void transition(struct fixture *f,unsigned type){
 lock(f);struct trl_freeze frozen;assert(trl_close(&f->lease)&&trl_seal(&f->lease,&frozen));
 uint64_t epoch=f->lease.type_epoch+1;
 for(struct trl_binding *b=f->lease.bindings;b;b=b->next)if(b->token_live){
  struct trl_token token,prepared;assert(trl_token_read(&f->lease,b,&token));
  assert(trl_prepare_token(&f->lease,frozen,b,token,epoch,type,&prepared));
 }
 struct trl_transition_evidence e={.freeze=frozen,.new_epoch=epoch,.serial=100+epoch,
  .closed=3,.drained=3,.tlb_before=3,.cache_before=3,.tlb_after=3,.cache_after=3,.destination=type,
  .complete_alias_audit=true,.all_refills_revoked=true,.all_roots_invalidated=true,
  .host_break_before_make=true,.all_aliases_retyped=true,.owner_stamps_ready=true};
 assert(trl_reopen(&f->lease,frozen,&e));unlock(f);
}
static void retained(struct owner *o){
 assert(o->pool.live==3&&!o->frees&&!o->f->depth);
 assert(o->slots[0].typed.token_live&&!o->slots[0].directory.receipt.live);
 assert(o->slots[0].directory.item&&o->slots[0].directory.item->tree.root);
}
static void successes(void){
 for(unsigned cpu=0;cpu<2;cpu++)for(unsigned history=0;history<2;history++)for(unsigned uc=0;uc<2;uc++){
  struct fixture f;init(&f);f.cpu=cpu;struct owner *o=&f.owners[cpu];
  assert(nested_ept_contexts_select(&o->registry,0x1001e)==NESTED_CONTEXT_OK);
  if(history)resident(o,cpu);
  if(uc)transition(&f,TRL_UC);
  assert(nested_ept_contexts_destroy(&o->registry));
  assert(o->invepts==1&&o->flushes==2&&o->frees==3&&f.serial==1&&!f.lease.live_tokens);
  assert(o->completed[0].acked==(history?UINT64_C(1)<<cpu:0));
  assert(o->completed[0].invalidation.acked==(UINT64_C(1)<<cpu));
  lock(&f);assert(tr_local_completed_matches(&o->provider,&o->slots[0]));unlock(&f);
 }
 for(unsigned uc=0;uc<2;uc++){
  struct fixture f;init(&f);struct owner *o=&f.owners[0];
  assert(nested_ept_contexts_select(&o->registry,0x1001e)==NESTED_CONTEXT_OK);resident(o,0);
  if(uc)transition(&f,TRL_UC);
  o->interleave=true;
  assert(nested_ept_contexts_destroy(&o->registry)&&f.peer_steps==5&&f.serial==2);
  assert(o->slots[0].receipt_version>o->slots[0].evidence_version+1);
  assert(f.owners[1].slots[0].receipt_version<o->slots[0].receipt_version);
  struct trl_freeze frozen;struct trl_snapshot snapshots[2];size_t count=0;
  lock(&f);assert(trl_close(&f.lease)&&trl_seal(&f.lease,&frozen));
  assert(trl_snapshot_read(&f.lease,frozen,snapshots,2,&count)&&count==2);
  for(size_t n=0;n<count;n++)assert(!snapshots[n].scan_allowed&&snapshots[n].root.receipt);
  unlock(&f);
 }
}
static void negatives(void){
 for(unsigned pre=0;pre<2;pre++)for(enum mutation m=DOMAIN;m<LAST_MUTATION;m++){
  struct fixture f;init(&f);struct owner *o=&f.owners[0];
  assert(nested_ept_contexts_select(&o->registry,0x1001e)==NESTED_CONTEXT_OK);resident(o,0);
  o->mutation=m;o->mutate_before=pre;
  assert(!nested_ept_contexts_destroy(&o->registry));retained(o);
  assert(o->invepts==(pre?0u:1u)&&!o->completed[0].complete);
 }
 for(enum mutation m=DOMAIN;m<=OP_NONCE;m++){
  struct fixture f;init(&f);struct owner *o=&f.owners[0];
  assert(nested_ept_contexts_select(&o->registry,0x1001e)==NESTED_CONTEXT_OK);
  o->mutation=m;o->corrupt_scope=true;
  assert(!nested_ept_contexts_destroy(&o->registry));retained(o);
  assert(!o->invepts&&!o->completed[0].complete&&!f.serial);
 }
 for(enum injection i=INVEPT_FAIL;i<=LOCK_POST;i++)for(unsigned history=0;history<2;history++){
  struct fixture f;init(&f);struct owner *o=&f.owners[0];
  assert(nested_ept_contexts_select(&o->registry,0x1001e)==NESTED_CONTEXT_OK);if(history)resident(o,0);
  o->injection=i;assert(!nested_ept_contexts_destroy(&o->registry));retained(o);
  bool published=i>=OUT_ACK&&i<=OUT_NONCE;
  assert(o->completed[0].complete==published&&f.serial==(published?1u:0u));
  assert(o->invepts==(i==LOCK_PRE?0u:1u));
 }
 for(unsigned cpu=0;cpu<2;cpu++){
  struct fixture f;init(&f);f.cpu=cpu;struct owner *o=&f.owners[cpu];
  assert(nested_ept_contexts_select(&o->registry,0x1001e)==NESTED_CONTEXT_OK);resident(o,cpu^1);
  assert(!nested_ept_contexts_destroy(&o->registry));retained(o);assert(!o->invepts&&!f.serial);
 }
}
static void retry_and_audit(void){
 struct fixture f;init(&f);struct owner *o=&f.owners[0];
 assert(nested_ept_contexts_select(&o->registry,0x1001e)==NESTED_CONTEXT_OK);
 o->injection=INVEPT_FAIL;assert(!nested_ept_contexts_destroy(&o->registry));retained(o);
 struct tr_registry_retire_scope old=o->pending_scope;o->injection=CLEAN;
 assert(nested_ept_contexts_destroy(&o->registry));
 assert(old.ticket.nonce==o->completed[0].scope.ticket.nonce);
 assert(old.operation_nonce!=o->completed[0].scope.operation_nonce);
 struct tr_local_completed saved=o->completed[0];struct td_receipt receipt=o->slots[0].directory.receipt;
 lock(&f);
 for(unsigned field=0;field<10;field++){
  o->completed[0]=saved;o->slots[0].directory.receipt=receipt;
  switch(field){
  case 0:o->completed[0].complete=false;break;
  case 1:o->completed[0].invalidation.cpu^=1;break;
  case 2:o->completed[0].invalidation.full_eptp^=1;break;
  case 3:o->completed[0].invalidation.success=false;break;
  case 4:o->completed[0].invalidation.acked^=3;break;
  case 5:o->completed[0].acked^=3;break;
  case 6:o->completed[0].scope.operation_nonce++;break;
  case 7:o->completed[0].scope.ticket.nonce++;break;
  case 8:o->completed[0].flush_serial++;break;
  case 9:o->slots[0].directory.receipt.commit_version++;break;
  }
  assert(!tr_local_completed_matches(&o->provider,&o->slots[0]));
 }
 o->completed[0]=saved;o->slots[0].directory.receipt=receipt;
 assert(tr_local_completed_matches(&o->provider,&o->slots[0]));unlock(&f);
}
int main(void){
 successes();negatives();retry_and_audit();
 puts("PASS N39B1 production local provider: exact unlocked INVEPT, full pre/post scope and CPU, zero history, retained failures, durable audit, peer version progress (host mocks only)");
 return 0;
}
