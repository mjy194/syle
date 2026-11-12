#include "typed_root_atomic.h"
#include "typed_root_atomic_maintenance.h"
#include "typed_root_source_replace.h"
static bool sr_command_take(struct tr_atomic_owner *,enum nested_context_operation);
static bool sr_command_admit(struct tr_atomic_owner *);
static bool sr_command_key(struct tr_atomic_owner *,uint64_t);
#define ROOT_MASK UINT64_C(0x000ffffffffff000)
static const struct nested_context_transactions *base(void) {return typed_root_registry_transactions();}
static void lock(struct tr_atomic_owner *o) {o->config.lock(o->config.context);}
static void unlock(struct tr_atomic_owner *o) {o->config.unlock(o->config.context);}
static bool identity_equal(struct td_identity a,struct td_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool catalog_equal(struct nrc_identity a,struct nrc_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;
}
static bool root_equal(struct nrc_root a,struct nrc_root b) {
 return catalog_equal(a.identity,b.identity)&&a.source==b.source&&a.owner==b.owner&&
  a.generation==b.generation&&a.lifetime==b.lifetime&&a.full_eptp==b.full_eptp&&a.root_gpa==b.root_gpa;
}
static bool cache_equal(struct nac_cache_identity a,struct nac_cache_identity b) {
 return a.owner==b.owner&&a.hpa==b.hpa&&a.full_eptp==b.full_eptp&&
  a.lifetime==b.lifetime&&a.generation==b.generation;
}
static bool mapping_equal(struct nested_ept_result a,struct nested_ept_result b) {
 return a.status==b.status&&a.l1_gpa==b.l1_gpa&&a.hpa==b.hpa&&
  a.effective12==b.effective12&&a.effective02==b.effective02&&a.leaf_resolved==b.leaf_resolved&&
  a.memory_explicit==b.memory_explicit&&a.ignore_pat==b.ignore_pat&&
  a.leaf_type==b.leaf_type&&a.walk_type==b.walk_type;
}
static bool local(struct tr_atomic_owner *o) {
 return o&&o->initialized&&o->config.current_cpu(o->config.context)==o->bridge->directory.owner&&
  (o->replacement?tsr_consumer_stopped(o):o->config.consumer_stopped(o->config.context));
}
static bool type_bound(const struct nested_root_catalog *c,const struct typed_root_lease *l) {
 return c&&l&&c->atomic_wb&&l->atomic_wb&&c->atomic_uc==l->atomic_uc&&
  l->type==(l->atomic_uc?TRL_UC:TRL_WB)&&
  (!c->atomic_uc||(!c->atomic_2m&&!c->atomic_1g&&c->type_epoch==l->type_epoch&&c->uc_hpa==l->backing.hpa));
}
static bool shared_healthy_locked(struct tr_atomic_owner *o) {
 struct typed_root_registry *b=o->bridge;
 return (!o->replacement||o->replacement->state!=TSR_POISONED)&&!o->poisoned&&!b->directory.faulted&&!b->lease->poisoned&&!b->directory.directory->poisoned&&
  b->atomic_wb&&b->root_scoped&&type_bound(o->config.catalog,b->lease)&&
  b->lease->type_epoch==b->directory.directory->type_epoch;
}
/* Owner-local registry state is only inspected by its owner or after the
 * shared operation ledger proves every owner mutation completed. */
static bool healthy_locked(struct tr_atomic_owner *o) {return shared_healthy_locked(o)&&!o->registry->poisoned;}
static void poison(struct tr_atomic_owner *o,struct nested_ept_context_item *item) {
 lock(o);o->poisoned=true;o->registry->poisoned=true;o->bridge->directory.faulted=true;
 if(item)item->tree.poisoned=true;
 trl_poison(o->bridge->lease);unlock(o);
}
static struct tr_atomic_binding *binding(struct tr_atomic_owner *o,void *token) {
 for(size_t i=0;i<o->config.slot_count;i++)
  if(token==&o->config.slots[i].directory)return &o->config.bindings[i];
 return NULL;
}
/* Owner-exclusive session metadata is stable while its pin is retained. */
static bool pin_identity(struct tr_atomic_owner *o,struct tr_atomic_binding *a) {
 return a&&a->bound&&!a->released&&a->pin.live&&a->pin_nonce&&a->pin.nonce==a->pin_nonce&&
  a->pin.catalog==o->config.catalog&&a->pin.authority_context==o->config.authority_context&&
  root_equal(a->root,a->pin.root)&&catalog_equal(a->root.identity,o->config.catalog_identity);
}
/* Source locks/callbacks never nest under directory metadata serialization. */
static bool pin_guard(struct tr_atomic_owner *o,struct tr_atomic_binding *a) {
 return pin_identity(o,a)&&nrc_guard(&a->pin,a->pin_nonce);
}
static bool source_guard(struct tr_atomic_owner *o,struct tr_atomic_binding *a) {
 struct nrc_root current;
 return pin_guard(o,a)&&
  nrc_find(o->config.catalog,o->config.catalog_identity,a->root.full_eptp,&current)&&
  root_equal(a->root,current);
}
static bool slot_locked(struct tr_atomic_owner *o,struct tr_atomic_binding *a,
 struct nested_ept_context_item *item) {
 if(!a||!a->slot||!item)return false;
 struct tr_registry_slot *s=a->slot;struct typed_root_registry *b=o->bridge;
 bool linked=false;for(struct tr_registry_slot *p=b->slots;p;p=p->next)if(p==s){linked=true;break;}
 return linked&&s->bridge==b&&s->directory.bridge==&b->directory&&s->directory.item==item&&
  !s->directory.forgotten&&s->directory.reserved&&item->transaction_token==&s->directory&&
  item->key==s->directory.virtual_key&&item->key==a->root.full_eptp&&(item->key&4095)==0x5e&&
  item->tree.root&&item->tree.hardware_ad&&(item->tree.eptp&4095)==0x5e&&
  item->tree.eptp==s->directory.record.full_eptp&&
  (item->tree.eptp&ROOT_MASK)==s->directory.record.hpa&&
  item->tree.generation==s->directory.record.cache_generation&&
  identity_equal(s->directory.identity,s->directory.record.identity)&&
  s->directory.identity.owner==b->directory.owner&&
  s->directory.identity.allocator_lifetime==s->typed.allocator_lifetime;
}
static bool operation_locked(struct tr_atomic_owner *o,struct td_operation *op,uint64_t nonce) {
 if(!nonce)return false;
 for(struct td_operation *p=o->bridge->directory.directory->ops;p;p=p->next)
  if(p==op)return p->live&&p->nonce==nonce&&p->directory==o->bridge->directory.directory;
 return false;
}
static bool fault_locked(struct tr_atomic_owner *o) {
 struct tr_atomic_fault *f=&o->fault;struct typed_root_registry *b=o->bridge;
 if(!healthy_locked(o)||!f->active||!f->admitted||!f->transient||f->owner!=o||
    b->directory.busy||b->refill_operation!=&f->operation||b->refill_nonce!=f->nonce||
    !f->binding||b->refill_slot!=f->binding->slot||
    !operation_locked(o,&f->operation,f->nonce))return false;
 struct tr_registry_slot *s=f->binding->slot;
 /* Match saved identities before dereferencing any cache/source path. */
 if(f->item!=o->registry->active||!identity_equal(f->identity,s->directory.identity)||!root_equal(f->source,f->binding->root)||
    f->virtual_key!=s->directory.virtual_key||f->type_epoch!=b->lease->type_epoch||
    !slot_locked(o,f->binding,f->item))return false;
 struct nac_cache_identity now={s->directory.identity.owner,s->directory.record.hpa,
  s->directory.record.full_eptp,s->directory.identity.allocator_lifetime,s->directory.record.cache_generation};
 return cache_equal(f->cache,now)&&!f->item->tree.poisoned&&
  s->directory.record.refills&&trl_admit(b->lease,&f->operation,f->nonce,&s->typed,
   f->type_token,b->lease->type,true,true);
}
static bool cache_guard(void *context,struct nac_cache_identity cache) {
 struct tr_atomic_owner *o=context;
 if(!local(o)||!cache_equal(cache,o->fault.cache))return false;
 lock(o);bool ok=fault_locked(o);unlock(o);
 return ok&&source_guard(o,o->fault.binding);
}
static bool mapping_admit(void *context,struct tr_registry_slot *s,const struct nested_ept_result *r) {
 struct tr_atomic_owner *o=context;struct tr_atomic_fault *f=&o->fault;
 return fault_locked(o)&&s==f->binding->slot&&f->permit.active&&
  f->ticket.source_nonce&&f->ticket.updater_nonce&&r&&r->memory_explicit&&
  r->walk_type==TRL_WB&&r->leaf_type==(r->hpa==o->bridge->backing_hpa?o->bridge->lease->type:TRL_WB)&&r->ignore_pat;
}
static bool pending(void *context,struct tr_registry_slot *s,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;struct tr_atomic_binding *a=binding(o,&s->directory);
 return !o->fault.active&&a&&a->slot==s&&pin_identity(o,a)&&slot_locked(o,a,item);
}
static bool before_forget(void *context,struct tr_registry_slot *s,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;struct tr_atomic_binding *a=binding(o,&s->directory);
 if(!local(o)||o->fault.active||!a||a->slot!=s||!a->bound)return false;
 lock(o);
 bool ready=healthy_locked(o)&&slot_locked(o,a,item)&&!item->pins&&!s->directory.record.pins&&
  !s->directory.record.refills&&s->directory.record.state==TD_RETIRED&&
  s->typed.state==TRL_RETIRED&&!s->typed.token_live&&
  s->typed.receipt==&s->directory.receipt&&s->receipt_version==s->directory.receipt.commit_version&&
  td_receipt_matches(o->bridge->directory.directory,&s->directory.receipt,s->directory.retirement);
 unlock(o);
 if(!ready)return false;
 if(!a->released) {
  if(!pin_guard(o,a)||!nrc_end(&a->pin,a->pin_nonce))return false;
  a->released=true;
 }
 return true;
}
static bool admission_locked(void *context) {
 struct tr_atomic_owner *o=context;
 return (!o->maintenance||o->maintenance->state==TAM_OPEN)&&(!o->replacement||o->replacement->state==TSR_OPEN||o->replacement->state==TSR_RELEASED);
}
static bool transaction_admission_locked(void *context) {
 struct tr_atomic_owner *o=context;return sr_command_admit(o)&&(!o->maintenance||o->maintenance->state==TAM_OPEN);
}
static bool begin(void *context,enum nested_context_operation kind) {
 struct tr_atomic_owner *o=context;
 return sr_command_take(o,kind)&&local(o)&&!o->poisoned&&!o->fault.active&&!o->entry_active&&typed_root_registry_begin_guarded(o->bridge,kind,transaction_admission_locked,o);
}
static void end(void *context,enum nested_context_operation kind) {
 struct tr_atomic_owner *o=context;base()->end(o->bridge,kind);
 lock(o);if(o->bridge->directory.faulted||o->registry->poisoned)o->poisoned=true;unlock(o);
}
static bool cancel(void *context,void *token) {
 struct tr_atomic_owner *o=context;struct tr_atomic_binding *a=binding(o,token);
 if(!a||!a->bound)return false;
 if(!a->released) {
  if(!nrc_end(&a->pin,a->pin_nonce)){poison(o,NULL);return false;}
  a->released=true;
 }
 return base()->cancel(o->bridge,token);
}
static bool reserve(void *context,uint64_t key,void **out) {
 struct tr_atomic_owner *o=context;struct nrc_root root;
 if(!out||!sr_command_key(o,key)||!local(o)||!nrc_find(o->config.catalog,o->config.catalog_identity,key,&root))return false;
 void *token=NULL;
 if(!base()->reserve(o->bridge,key,&token))return false;
 struct tr_atomic_binding *a=binding(o,token);
 if(!a||a->bound){poison(o,NULL);return false;}
 *a=(struct tr_atomic_binding){.slot=NULL,.root=root};
 for(size_t i=0;i<o->config.slot_count;i++)if(token==&o->config.slots[i].directory)a->slot=&o->config.slots[i];
 if(!nrc_begin(o->config.catalog,root,o->config.authority_context,&a->pin,&a->pin_nonce)) {
  if(!base()->cancel(o->bridge,token))poison(o,NULL);
  return false;
 }
 a->bound=true;*out=token;return true;
}
static bool attach(void *context,void *token,struct nested_ept_context_item *item,bool init_ok) {
 struct tr_atomic_owner *o=context;struct tr_atomic_binding *a=binding(o,token);
 /* Attach even a failed initialization so all actual ownership is recorded. */
 bool source_ok=source_guard(o,a);
 bool attached=base()->attach(o->bridge,token,item,init_ok);
 return source_ok&&attached;
}
static void retain(void *context,void *token,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;base()->retain(o->bridge,token,item);
}
static bool publish(void *context,void *token,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;
 return local(o)&&source_guard(o,binding(o,token))&&base()->publish(o->bridge,token,item);
}
static bool reset(void *context,void *token,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;
 return base()->reset(o->bridge,token,item);
}
static bool retire(void *context,void *token,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;
 return base()->retire(o->bridge,token,item);
}
static bool select_guard(void *context,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;struct tr_atomic_binding *a=binding(o,item->transaction_token);
 if(!sr_command_key(o,item->key)||!local(o)||!source_guard(o,a))return false;
 lock(o);struct trl_token token;struct typed_root_registry *b=o->bridge;
 bool ok=healthy_locked(o)&&!o->fault.active&&b->directory.busy&&slot_locked(o,a,item)&&
  trl_token_read(b->lease,&a->slot->typed,&token)&&
  trl_admit(b->lease,&b->directory.operation,b->directory.nonce,&a->slot->typed,token,b->lease->type,true,true);
 unlock(o);return ok;
}
static bool revoke(void *context,struct nested_ept_context_item *item) {
 struct tr_atomic_owner *o=context;struct tr_atomic_binding *a=binding(o,item->transaction_token);
 if(!local(o)||o->fault.active||!pin_guard(o,a))return false;
 lock(o);struct typed_root_registry *b=o->bridge;
 bool ok=healthy_locked(o)&&b->directory.busy&&operation_locked(o,&b->directory.operation,b->directory.nonce)&&
  slot_locked(o,a,item)&&!item->pins&&!a->slot->directory.record.pins;
 if(ok)for(size_t i=0;i<o->config.trace_count;i++) {
  struct tr_atomic_trace *t=&o->config.traces[i];
  if(!t->used||t->binding!=a)continue;
  t->usable=false;
  if(t->registered) {
   if(!td_refill_drop(b->directory.directory,&b->directory.operation,b->directory.nonce,
       &a->slot->directory.record,a->slot->directory.identity)){ok=false;break;}
   t->registered=false;
  }
 }
 if(ok)ok=!a->slot->directory.record.refills;
 unlock(o);return ok;
}
static const struct nested_context_transactions transactions={begin,end,reserve,cancel,attach,retain,publish,reset,retire};
static const struct nested_context_guards guards={select_guard,revoke,revoke};
bool tr_atomic_init(struct tr_atomic_owner *o,struct typed_root_registry *b,
 struct nested_ept_contexts *r,struct typed_root_lease *l,uint64_t owner,
 const struct tr_registry_root_provider *provider,void *provider_context,const struct tr_atomic_config *c) {
 if(!o||!b||!r||!l||!c||!c->catalog||!c->catalog->atomic_wb||!c->gate||!c->slots||!c->bindings||
    !c->slot_count||!c->traces||!c->trace_count||!c->lock||!c->unlock||!c->current_cpu||
    !c->consumer_stopped||!catalog_equal(c->catalog_identity,c->catalog->identity)||
    c->stamp.domain_id!=c->catalog_identity.domain||c->stamp.incarnation!=c->catalog_identity.incarnation||
    c->stamp.context_id!=c->catalog_identity.catalog||c->stamp.lifetime!=c->catalog_identity.lifetime||
    !c->stamp.epoch||!type_bound(c->catalog,l)||!l->directory||
    l->domain!=c->catalog_identity.domain||l->incarnation!=c->catalog_identity.incarnation||
    l->directory->domain!=l->domain||l->directory->incarnation!=l->incarnation||
    !r->initialized||!r->hardware_ad||r->mixed_ad||r->items||r->count||r->active||r->tick||
    r->transactions||r->guards||r->poisoned||owner>=64)return false;
 for(size_t i=0;i<c->slot_count;i++)if(c->bindings[i].bound||c->bindings[i].pin.live)return false;
 for(size_t i=0;i<c->trace_count;i++)if(c->traces[i].used||c->traces[i].registered)return false;
 *o=(struct tr_atomic_owner){.bridge=b,.registry=r,.config=*c};
 const struct tr_registry_atomic_hooks hooks={mapping_admit,pending,before_forget};
 if(!(l->atomic_uc?typed_root_registry_init_atomic_uc:typed_root_registry_init_atomic_wb)(b,l,owner,l->backing.hpa,provider,provider_context,&hooks,o)||
    !typed_root_registry_set_sync(b,c->lock,c->unlock,c->context)||
    !nested_ept_contexts_set_transactions(r,&transactions,o)||
    !nested_ept_contexts_set_guards(r,&guards,o))return false;
 o->initialized=true;return true;
}
static bool fault_begin(struct tr_atomic_owner *o,struct nested_ept_context_item *item,uint64_t page) {
 struct tr_atomic_fault *f=&o->fault;struct typed_root_registry *b=o->bridge;
 if(!local(o)||f->active)return false;
 lock(o);
 struct tr_atomic_binding *a=(item&&item==o->registry->active)?binding(o,item->transaction_token):NULL;struct trl_token token;
 bool ok=healthy_locked(o)&&admission_locked(o)&&item==o->registry->active&&!b->directory.busy&&!b->refill_operation&&a&&a->bound&&!a->released&&
  slot_locked(o,a,item)&&!item->tree.poisoned&&trl_token_read(b->lease,&a->slot->typed,&token);
 if(ok) {
  *f=(struct tr_atomic_fault){.owner=o,.binding=a,.item=item,.gpa=page,.active=true,
   .type_epoch=b->lease->type_epoch,.virtual_key=item->key,.identity=a->slot->directory.identity,
   .type_token=token,.source=a->root,.cache={a->slot->directory.identity.owner,
    a->slot->directory.record.hpa,item->tree.eptp,a->slot->directory.identity.allocator_lifetime,item->tree.generation}};
  ok=td_begin(b->directory.directory,&f->operation,&f->nonce);
  if(ok) {
   f->admitted=true;
   ok=trl_admit(b->lease,&f->operation,f->nonce,&a->slot->typed,token,b->lease->type,true,true)&&
    td_refill_add(b->directory.directory,&f->operation,f->nonce,&a->slot->directory.record,a->slot->directory.identity);
   if(ok) {
    f->transient=true;b->refill_operation=&f->operation;b->refill_slot=a->slot;b->refill_nonce=f->nonce;
   }
  }
 }
 unlock(o);return ok;
}
bool tr_atomic_fault_cleanup(struct tr_atomic_owner *o) {
 if(!o||!o->initialized)return false;
 struct tr_atomic_fault *f=&o->fault;struct typed_root_registry *b=o->bridge;
 if(!f->active)return true;
 if(f->permit.active&&!nac_end(&f->permit,f->ticket))return false;
 lock(o);
 bool ok=true;
 if(f->transient) {
  struct tr_registry_slot *s=f->binding->slot;
  ok=td_refill_drop(b->directory.directory,&f->operation,f->nonce,&s->directory.record,f->identity);
  if(ok)f->transient=false;
 }
 if(ok&&f->admitted) {
  ok=td_end(b->directory.directory,&f->operation,f->nonce);
  if(ok)f->admitted=false;
 }
 if(ok) {
  if(b->refill_operation==&f->operation){b->refill_operation=NULL;b->refill_slot=NULL;b->refill_nonce=0;}
  f->active=false;
 }
 unlock(o);return ok;
}
static struct tr_atomic_trace *find_trace(struct tr_atomic_owner *o,struct tr_atomic_binding *a,uint64_t page) {
 struct tr_atomic_trace *found=NULL;
 for(size_t i=0;i<o->config.trace_count;i++) {
  struct tr_atomic_trace *t=&o->config.traces[i];
  if(t->used&&t->binding==a&&t->trace.gpa==page&&t->usable&&t->registered) {
   if(found)return NULL;
   found=t;
  }
 }
 return found;
}
static bool register_trace(struct tr_atomic_owner *o) {
 struct tr_atomic_fault *f=&o->fault;
 struct tr_atomic_trace *t=NULL;
 for(size_t i=0;i<o->config.trace_count;i++)if(!o->config.traces[i].used){t=&o->config.traces[i];break;}
 if(!t)return false;
 *t=(struct tr_atomic_trace){.binding=f->binding,.trace=f->fresh,.used=true};f->record=t;
 lock(o);struct tr_registry_slot *s=f->binding->slot;
 bool ok=fault_locked(o)&&td_refill_add(o->bridge->directory.directory,&f->operation,f->nonce,
  &s->directory.record,f->identity);
 if(ok){t->registered=true;t->usable=true;}
 unlock(o);return ok;
}
enum tr_atomic_status tr_atomic_fault(struct tr_atomic_owner *o,
 struct nested_ept_context_item *item,uint64_t gpa,uint8_t access,struct nested_ept_result *out) {
 if(!o||!o->initialized||!out||!access||(access&~7u)||gpa>=(UINT64_C(1)<<o->registry->maxphys))return TR_ATOMIC_REJECTED;
 *out=(struct nested_ept_result){.status=NESTED_EPT_UNSUPPORTED};
 uint64_t page=gpa&~UINT64_C(4095);enum tr_atomic_status status=TR_ATOMIC_FATAL;
 if(!fault_begin(o,item,page)) {
  if(!o->fault.active)return TR_ATOMIC_REJECTED;
  goto finish;
 }
 struct tr_atomic_fault *f=&o->fault;
 if(!source_guard(o,f->binding)||!nac_begin(o->config.catalog,f->source,o->config.authority_context,
    o->config.gate,o->config.stamp,f->cache,cache_guard,o,&f->permit,&f->ticket))goto finish;
 struct nested_ept_result cached;
 enum nested_sparse_lookup lookup=nested_ept_sparse_lookup(&item->tree,page,access,&cached);
 if(lookup==NESTED_SPARSE_HIT) {
  bool tracked=false;
  f->record=find_trace(o,f->binding,page);
  if(!(access&2)||cached.status!=NESTED_EPT_OK||!f->record||
     !nested_ept_sparse_query_tracking(&item->tree,page,&tracked)||!tracked||
     !mapping_equal(cached,f->record->trace.translation)||
     !nac_trace_guard(&f->permit,f->ticket,page,&f->record->trace))goto complete;
  struct nested_ept_atomic_result walk=nac_walk(&f->permit,f->ticket,page,access,&f->fresh);
  *out=walk.translation;
  if(walk.status!=NEA_DONE||out->status!=NESTED_EPT_OK||
     !mapping_equal(*out,cached)||!mapping_equal(*out,f->record->trace.translation)||
     !nac_guard(&f->permit,f->ticket)||!nested_ept_sparse_promote_write(&item->tree,page))goto complete;
  f->record->trace=f->fresh;status=TR_ATOMIC_PROMOTED;
 }else if(lookup==NESTED_SPARSE_MISS) {
  struct nested_ept_atomic_result walk=nac_walk(&f->permit,f->ticket,page,access,&f->fresh);
  *out=walk.translation;
  if(walk.status!=NEA_DONE)goto complete;
  if(out->status!=NESTED_EPT_OK){status=TR_ATOMIC_DENIED;goto complete;}
  if(!register_trace(o)||!nac_guard(&f->permit,f->ticket))goto complete;
  bool trap=(out->effective02&2)&&!(access&2);
  if(nested_ept_sparse_fill_tracked(&item->tree,page,out,trap)!=NESTED_SPARSE_OK)goto complete;
  status=TR_ATOMIC_FILLED;
 }
complete:
 /* Complete's failed final guard stays fatal even if it released all permits. */
 if(!nac_complete(&f->permit,f->ticket))status=TR_ATOMIC_FATAL;
finish:
 if(!tr_atomic_fault_cleanup(o))status=TR_ATOMIC_FATAL;
 if(status==TR_ATOMIC_FATAL){o->fault.fatal=true;poison(o,item);}
 return status;
}

static bool entry_check(struct tr_atomic_owner *o,struct nested_ept_context_item *item,uint64_t eptp,bool resident) {
 if(!local(o)||!item||o->fault.active||o->entry_active||o->entry_operation.live||
    item!=o->registry->active||(resident&&!item->pins)||eptp!=item->tree.eptp)return false;
 struct tr_atomic_binding *a=binding(o,item->transaction_token);
 if(!source_guard(o,a)||!ad_updater_gate_open(o->config.gate,o->config.stamp))return false;
 lock(o);struct typed_root_registry *b=o->bridge;struct typed_directory *d=b->directory.directory;
 /* A closed gate denies a not-yet-admitted entry without changing state.
  * An already admitted entry racing close still fails its final guard below. */
 if(!admission_locked(o)){unlock(o);return false;}
 struct trl_token token;
 bool ok=healthy_locked(o)&&admission_locked(o)&&!d->closed&&!d->sealed&&!b->directory.busy&&!b->refill_operation&&
  !o->fault.active&&slot_locked(o,a,item)&&!item->tree.poisoned&&
  item->pins==a->slot->directory.record.pins&&a->slot->directory.record.state==TD_LIVE&&
  trl_token_read(b->lease,&a->slot->typed,&token)&&td_begin(d,&o->entry_operation,&o->entry_nonce);
 if(ok){
  o->entry_active=true;
  ok=trl_admit(b->lease,&o->entry_operation,o->entry_nonce,&a->slot->typed,token,b->lease->type,true,true)&&
   (!resident||td_resident(d,&o->entry_operation,o->entry_nonce,&a->slot->directory.record,
    a->slot->directory.identity,(unsigned)b->directory.owner));
 }
 unlock(o);
 if(ok)ok=local(o)&&source_guard(o,a)&&ad_updater_gate_open(o->config.gate,o->config.stamp);
 lock(o);
 /* Replacement close may cancel an already admitted entry after its final
  * source guard. Drain its operation without poisoning, but never return
  * physical entry authority. The separate consumer gate is already closed. */
 bool cancelled=o->replacement&&(o->replacement->state==TSR_STOPPING||o->replacement->state==TSR_BOUND||o->replacement->state==TSR_DRAINING)&&!admission_locked(o);
 if(ok)ok=healthy_locked(o)&&(admission_locked(o)||cancelled)&&!d->closed&&!d->sealed&&!b->directory.busy&&!b->refill_operation&&
  !o->fault.active&&item==o->registry->active&&slot_locked(o,a,item)&&!item->tree.poisoned&&
  eptp==item->tree.eptp&&(!resident||item->pins)&&item->pins==a->slot->directory.record.pins&&
  a->slot->directory.record.state==TD_LIVE&&
  trl_admit(b->lease,&o->entry_operation,o->entry_nonce,&a->slot->typed,token,b->lease->type,true,true);
 if(o->entry_active){
  bool ended=td_end(d,&o->entry_operation,o->entry_nonce);
  if(ended){o->entry_active=false;o->entry_nonce=0;}
  ok=ok&&ended;
 }
 if(!ok){o->poisoned=true;o->registry->poisoned=true;b->directory.faulted=true;trl_poison(b->lease);}
 unlock(o);return ok&&!cancelled;
}

bool tr_atomic_entry(struct tr_atomic_owner *o,struct nested_ept_context_item *item,uint64_t eptp) {
 return entry_check(o,item,eptp,true);
}
bool tr_atomic_ready(struct tr_atomic_owner *o) {
 if(!o||!o->initialized||!o->registry->active)return false;
 struct nested_ept_context_item *item=o->registry->active;
 return entry_check(o,item,item->tree.eptp,false);
}

#include "typed_root_atomic_maintenance.inc"

#include "typed_root_source_replace.inc"
#include "typed_root_source_successor.inc"
