#include "typed_root_registry.h"
#define ROOT_MASK UINT64_C(0x000ffffffffff000)
static const struct nested_context_transactions *base(void) {return td_registry_transactions();}
static void lock(struct typed_root_registry *b) {
 if(b&&b->metadata_lock)b->metadata_lock(b->metadata_context);
}
static void unlock(struct typed_root_registry *b) {
 if(b&&b->metadata_unlock)b->metadata_unlock(b->metadata_context);
}
static bool identity_equal(struct td_identity a,struct td_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool ticket_equal(struct td_retire_ticket a,struct td_retire_ticket b) {
 return identity_equal(a.identity,b.identity)&&a.full_eptp==b.full_eptp&&a.hpa==b.hpa&&
  a.cache_generation==b.cache_generation&&a.history==b.history&&a.type_epoch==b.type_epoch&&a.nonce==b.nonce;
}
static void fault(struct typed_root_registry *b) {
 b->directory.faulted=true;trl_poison(b->lease);
}
static bool healthy(struct typed_root_registry *b) {
 return b&&b->lease&&!b->directory.faulted&&!b->lease->poisoned&&
  !b->directory.directory->poisoned&&b->lease->type_epoch==b->directory.directory->type_epoch;
}
static struct tr_registry_slot *find(struct typed_root_registry *b,void *token) {
 if(!b||!token)return NULL;
 for(struct tr_registry_slot *s=b->slots;s;s=s->next)
  if(&s->directory==token)return s->bridge==b?s:NULL;
 return NULL;
}
static bool reserve_storage(void *context,struct td_registry_slot **out,uint64_t *lifetime) {
 struct typed_root_registry *b=context;struct tr_registry_slot *s=NULL;uint64_t life=0;
 if(!healthy(b)||!b->directory.busy||!out||!lifetime||
    b->lease->reserved>=b->lease->budget||!(b->root_scoped?b->root_provider.reserve(b->provider_context,&s,&life):
       b->provider.reserve(b->provider_context,&s,&life)))return false;
 if(!s||!life){fault(b);return false;}
 for(struct tr_registry_slot *p=b->slots;p;p=p->next)if(p==s){fault(b);return false;}
 *s=(struct tr_registry_slot){.next=b->slots,.bridge=b};b->slots=s;
 if(!trl_reserve(b->lease,&b->directory.operation,b->directory.nonce,&s->typed,b->directory.owner,life))return false;
 *out=&s->directory;*lifetime=life;return true;
}
/* The legacy provider entry is mandatory at construction, but this wrapper's
 * retire stages independently preserve the typed proof before td_forget. */
static bool unused_evidence(void *context,struct td_registry_slot *slot,
 struct td_retire_ticket ticket,uint64_t *acked,uint64_t *serial) {
 (void)context;(void)slot;(void)ticket;(void)acked;(void)serial;return false;
}
static bool begin_locked(void *context,enum nested_context_operation kind) {
 struct typed_root_registry *b=context;
 return healthy(b)&&!b->refill_operation&&base()->begin(&b->directory,kind);
}
bool typed_root_registry_begin_guarded(struct typed_root_registry *b,
 enum nested_context_operation kind,bool (*predicate)(void *),void *context) {
 if(!b||!predicate)return false;
 lock(b);bool ok=predicate(context)&&begin_locked(b,kind);unlock(b);return ok;
}
static void end_locked(void *context,enum nested_context_operation kind) {
 struct typed_root_registry *b=context;
 if(!b)return;
 base()->end(&b->directory,kind);
 if(b->directory.faulted)trl_poison(b->lease);
}
static bool reserve_locked(void *context,uint64_t key,void **out) {
 struct typed_root_registry *b=context;
 if(!healthy(b)||!out||(b->atomic_wb&&(key&4095)!=0x5e))return false;
 struct tr_registry_slot *previous=b->slots;
 if(!base()->reserve(&b->directory,key,out)) {
  if(b->slots!=previous&&b->slots->typed.lease==b->lease)
   if(!trl_cancel(b->lease,&b->directory.operation,b->directory.nonce,&b->slots->typed))fault(b);
  return false;
 }
 struct tr_registry_slot *s=find(b,*out);
 if(!s||!trl_bind_candidate(b->lease,&b->directory.operation,b->directory.nonce,
    &s->typed,&s->directory.record,s->directory.identity)){fault(b);return false;}
 return true;
}
static bool cancel_locked(void *context,void *token) {
 struct typed_root_registry *b=context;struct tr_registry_slot *s=find(b,token);
 if(!s||!healthy(b)||!base()->cancel(&b->directory,token))return false;
 if(!trl_cancel(b->lease,&b->directory.operation,b->directory.nonce,&s->typed)){fault(b);return false;}
 return true;
}
static bool mapping_admitted_locked(void *context,const struct nested_ept_result *mapped) {
 struct tr_registry_slot *s=context;struct typed_root_registry *b=s?s->bridge:NULL;struct trl_token token;
 return healthy(b)&&b->refill_operation&&b->refill_slot==s&&s->directory.item&&!s->directory.forgotten&&mapped&&
  mapped->memory_explicit&&mapped->walk_type==TRL_WB&&
  mapped->leaf_type==(mapped->hpa==b->backing_hpa?b->lease->type:TRL_WB)&&
  mapped->ignore_pat&&(mapped->hpa!=b->backing_hpa||!(mapped->effective02&4))&&
  s->directory.item->tree.hardware_ad==b->atomic_wb&&
  (!b->atomic_wb||b->atomic_hooks.admit(b->atomic_context,s,mapped))&&
  trl_token_read(b->lease,&s->typed,&token)&&
  trl_admit(b->lease,b->refill_operation,b->refill_nonce,&s->typed,token,
            b->lease->type,mapped->ignore_pat,s->directory.item->tree.hardware_ad);
}
static bool mapping_admitted(void *context,const struct nested_ept_result *mapped) {
 struct tr_registry_slot *s=context;struct typed_root_registry *b=s?s->bridge:NULL;
 lock(b);bool ok=mapping_admitted_locked(context,mapped);unlock(b);return ok;
}
static bool attach_locked(void *context,void *token,struct nested_ept_context_item *item,bool init_ok) {
 struct typed_root_registry *b=context;struct tr_registry_slot *s=find(b,token);
 if(!s||!healthy(b)||!base()->attach(&b->directory,token,item,init_ok))return false;
 if(!trl_attach(b->lease,&b->directory.operation,b->directory.nonce,&s->typed))return false;
 if(!init_ok)return true;
 return item->tree.hardware_ad==b->atomic_wb&&
  (!b->atomic_wb||((item->key&4095)==0x5e&&(item->tree.eptp&4095)==0x5e))&&
  (b->lease->atomic_uc?nested_ept_sparse_set_typed_admission_atomic_uc:nested_ept_sparse_set_typed_admission)(&item->tree,mapping_admitted,s);
}
static void retain_locked(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;struct tr_registry_slot *s=find(b,token);
 if(!s||!b->directory.busy){if(b)fault(b);return;}
 base()->retain(&b->directory,token,item);
 if(b->directory.faulted||
    !trl_retain(b->lease,&b->directory.operation,b->directory.nonce,&s->typed))fault(b);
}
static bool publish_locked(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;struct tr_registry_slot *s=find(b,token);
 if(!s||!healthy(b)||s->directory.item!=item||item->tree.typed_admit!=mapping_admitted||
    item->tree.typed_context!=s||
    !trl_publish_ready(b->lease,&b->directory.operation,b->directory.nonce,&s->typed))return false;
 if(!base()->publish(&b->directory,token,item))return false;
 return trl_publish(b->lease,&b->directory.operation,b->directory.nonce,&s->typed);
}
static bool reset_locked(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;struct tr_registry_slot *s=find(b,token);
 return s&&healthy(b)&&s->directory.item==item&&base()->reset(&b->directory,token,item);
}
static bool proof_matches(struct tr_registry_slot *s) {
 struct td_receipt *r=&s->directory.receipt;struct typed_root_registry *b=s->bridge;
 return ticket_equal(s->evidence_ticket,s->directory.retirement)&&
  s->receipt_version&&s->receipt_version==r->commit_version&&s->evidence_operation_nonce&&
  (b->root_scoped?s->receipt_version>s->evidence_version:
   s->evidence_version!=UINT64_MAX&&s->receipt_version==s->evidence_version+1)&&
  td_receipt_matches(b->directory.directory,r,s->evidence_ticket)&&
  r->acked==s->evidence_acked&&r->flush_serial==s->evidence_serial;
}
static bool item_matches(struct tr_registry_slot *s,struct nested_ept_context_item *item,
 struct td_retire_ticket t) {
 return s->directory.item==item&&!s->directory.forgotten&&item&&!item->pins&&
  item->transaction_token==&s->directory&&item->key==s->directory.virtual_key&&
  item->tree.root&&item->tree.eptp==t.full_eptp&&item->tree.generation==t.cache_generation&&
  item->tree.hardware_ad==s->bridge->atomic_wb&&
  (!s->bridge->atomic_wb||((item->key&4095)==0x5e&&(item->tree.eptp&4095)==0x5e))&&identity_equal(s->directory.identity,t.identity);
}
static bool retire_locked(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;struct tr_registry_slot *s=find(b,token);
 if(!s||!healthy(b)||!b->directory.busy||s->directory.item!=item||item->pins||s->directory.forgotten)return false;
 struct td_registry_slot *d=&s->directory;struct typed_directory *directory=b->directory.directory;
 struct td_operation *op=&b->directory.operation;uint64_t nonce=b->directory.nonce;
 if(d->record.state!=TD_RETIRED) {
  if(d->record.state!=TD_RETIRING&&
     !td_retire_begin(directory,op,nonce,&d->record,d->identity,&d->retirement))return false;
  struct td_retire_ticket ticket=d->retirement;uint64_t version=directory->version;
  if(!item_matches(s,item,ticket)||!td_retire_pending(directory,op,nonce,&d->record,d->identity,ticket))return false;
  struct tr_registry_flush_evidence legacy={0};
  struct tr_registry_retire_scope scope={ticket,nonce};struct tr_registry_root_evidence evidence={0};
  /* The admitted operation retains the root across the unlocked provider.
   * Peer bridges may mutate unrelated roots, but cannot advance the type
   * epoch or mutate this RETIRING root through ordinary admission APIs. */
  unlock(b);
  bool supplied=b->root_scoped?
   b->root_provider.flush_evidence(b->provider_context,s,scope,&evidence):
   b->provider.flush_evidence(b->provider_context,s,ticket,version,&legacy);
  lock(b);
  if(!b->root_scoped) {
   evidence=(struct tr_registry_root_evidence){.scope={legacy.ticket,nonce},
    .acked=legacy.acked,.flush_serial=legacy.flush_serial};
   if(legacy.directory_version!=version||directory->version!=version)return false;
  }
  if(!supplied||!healthy(b)||!b->directory.busy||b->directory.nonce!=nonce||
     evidence.scope.operation_nonce!=nonce||!ticket_equal(ticket,d->retirement)||
     !ticket_equal(evidence.scope.ticket,ticket)||!item_matches(s,item,ticket)||
     !td_retire_pending(directory,op,nonce,&d->record,d->identity,ticket)||
     evidence.acked!=ticket.history||!evidence.flush_serial)return false;
  if(!td_retire_commit(directory,op,nonce,&d->record,d->identity,ticket,
       evidence.acked,evidence.flush_serial,&d->receipt))return false;
  s->evidence_ticket=evidence.scope.ticket;s->evidence_version=version;
  s->evidence_operation_nonce=nonce;s->evidence_acked=evidence.acked;
  s->evidence_serial=evidence.flush_serial;s->receipt_version=d->receipt.commit_version;
 }
 if(!proof_matches(s)||!item_matches(s,item,d->retirement))return false;
 if(s->typed.state!=TRL_RETIRED) {
  struct trl_token typed_token;
  if(!trl_token_read(b->lease,&s->typed,&typed_token)||
     !trl_retire(b->lease,op,nonce,&s->typed,typed_token,d->retirement,&d->receipt,s->receipt_version))return false;
 }
 if(b->atomic_wb) {
  unlock(b);bool cleaned=b->atomic_hooks.before_forget(b->atomic_context,s,item);lock(b);
  if(!cleaned||!healthy(b)||!b->directory.busy||b->directory.nonce!=nonce||
     !proof_matches(s)||!item_matches(s,item,d->retirement))return false;
 }
 if(!td_forget(directory,op,nonce,&d->record,d->identity))return false;
 d->forgotten=true;d->item=NULL;return true;
}
static bool begin(void *context,enum nested_context_operation kind) {
 struct typed_root_registry *b=context;lock(b);
 bool ok=begin_locked(context,kind);unlock(b);return ok;
}
static bool reserve(void *context,uint64_t key,void **out) {
 struct typed_root_registry *b=context;lock(b);
 bool ok=reserve_locked(context,key,out);unlock(b);return ok;
}
static bool cancel(void *context,void *token) {
 struct typed_root_registry *b=context;lock(b);
 bool ok=cancel_locked(context,token);unlock(b);return ok;
}
static bool attach(void *context,void *token,struct nested_ept_context_item *item,bool init_ok) {
 struct typed_root_registry *b=context;lock(b);
 bool ok=attach_locked(context,token,item,init_ok);unlock(b);return ok;
}
static bool publish(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;lock(b);
 bool ok=publish_locked(context,token,item);unlock(b);return ok;
}
static bool reset(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;lock(b);
 bool ok=reset_locked(context,token,item);unlock(b);return ok;
}
static bool retire(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;lock(b);
 bool ok=retire_locked(context,token,item);unlock(b);return ok;
}
static void end(void *context,enum nested_context_operation kind) {
 struct typed_root_registry *b=context;lock(b);end_locked(context,kind);unlock(b);
}
static void retain(void *context,void *token,struct nested_ept_context_item *item) {
 struct typed_root_registry *b=context;lock(b);retain_locked(context,token,item);unlock(b);
}
static const struct nested_context_transactions transactions={begin,end,reserve,cancel,attach,retain,publish,reset,retire};
const struct nested_context_transactions *typed_root_registry_transactions(void) {return &transactions;}
static bool init(struct typed_root_registry *b,struct typed_root_lease *l,
 uint64_t owner,uint64_t backing,void *context) {
 if(!b||!l||!l->directory||l->poisoned||!backing||backing!=l->backing.hpa||(backing&4095)||(backing&~ROOT_MASK))return false;
 *b=(struct typed_root_registry){.lease=l,.provider_context=context,.backing_hpa=backing};
 const struct td_registry_provider old_provider={reserve_storage,unused_evidence};
 return td_registry_bridge_init(&b->directory,l->directory,owner,&old_provider,b);
}
bool typed_root_registry_init(struct typed_root_registry *b,struct typed_root_lease *l,
 uint64_t owner,uint64_t backing,const struct tr_registry_provider *provider,void *context) {
 if(!l||l->atomic_wb||!provider||!provider->reserve||!provider->flush_evidence||!init(b,l,owner,backing,context))return false;
 b->provider=*provider;return true;
}
bool typed_root_registry_init_root_scoped(struct typed_root_registry *b,struct typed_root_lease *l,
 uint64_t owner,uint64_t backing,const struct tr_registry_root_provider *provider,void *context) {
 if(!l||l->atomic_wb||!provider||!provider->reserve||!provider->flush_evidence||!init(b,l,owner,backing,context))return false;
 b->root_scoped=true;b->root_provider=*provider;return true;
}
static bool init_atomic(struct typed_root_registry *b,struct typed_root_lease *l,
 uint64_t owner,uint64_t backing,const struct tr_registry_root_provider *provider,void *context,
 const struct tr_registry_atomic_hooks *hooks,void *atomic_context,bool uc) {
 if(!l||!l->atomic_wb||l->atomic_uc!=uc||l->type!=(uc?TRL_UC:TRL_WB)||!provider||!provider->reserve||!provider->flush_evidence||
    !hooks||!hooks->admit||!hooks->pending||!hooks->before_forget||!init(b,l,owner,backing,context))return false;
 b->atomic_wb=true;b->root_scoped=true;b->root_provider=*provider;
 b->atomic_hooks=*hooks;b->atomic_context=atomic_context;return true;
}
bool typed_root_registry_init_atomic_wb(struct typed_root_registry *b,struct typed_root_lease *l,
 uint64_t owner,uint64_t backing,const struct tr_registry_root_provider *p,void *c,
 const struct tr_registry_atomic_hooks *h,void *a) {return init_atomic(b,l,owner,backing,p,c,h,a,false);}
bool typed_root_registry_init_atomic_uc(struct typed_root_registry *b,struct typed_root_lease *l,
 uint64_t owner,uint64_t backing,const struct tr_registry_root_provider *p,void *c,
 const struct tr_registry_atomic_hooks *h,void *a) {return init_atomic(b,l,owner,backing,p,c,h,a,true);}
bool typed_root_registry_set_sync(struct typed_root_registry *b,void (*enter)(void *),
 void (*leave)(void *),void *context) {
 if(!b||!enter||!leave||b->metadata_lock||b->metadata_unlock||b->slots||
    b->directory.busy||b->refill_operation||!healthy(b))return false;
 b->metadata_lock=enter;b->metadata_unlock=leave;b->metadata_context=context;return true;
}
static struct tr_registry_slot *refill_begin_locked(struct typed_root_registry *b,
 struct nested_ept_context_item *item,struct td_operation *op,uint64_t *nonce) {
 if(!healthy(b)||b->atomic_wb||!item||b->directory.busy||b->refill_operation)return NULL;
 struct tr_registry_slot *s=find(b,item->transaction_token);struct trl_token token;
 if(!s||s->directory.item!=item||s->directory.forgotten||item->tree.poisoned||
    item->tree.typed_admit!=mapping_admitted||item->tree.typed_context!=s||
    item->tree.generation!=s->directory.record.cache_generation||
    item->tree.eptp!=s->directory.record.full_eptp||
    !trl_token_read(b->lease,&s->typed,&token)||!td_begin(b->directory.directory,op,nonce))return NULL;
 if(!trl_admit(b->lease,op,*nonce,&s->typed,token,b->lease->type,true,item->tree.hardware_ad)||
    !td_refill_add(b->directory.directory,op,*nonce,&s->directory.record,s->directory.identity)) {
  if(!td_end(b->directory.directory,op,*nonce))fault(b);
  return NULL;
 }
 b->refill_operation=op;b->refill_slot=s;b->refill_nonce=*nonce;return s;
}
static bool refill_end_locked(struct typed_root_registry *b,struct tr_registry_slot *s,
 struct td_operation *op,uint64_t nonce) {
 b->refill_operation=NULL;b->refill_slot=NULL;b->refill_nonce=0;
 bool ok=td_refill_drop(b->directory.directory,op,nonce,&s->directory.record,s->directory.identity);
 if(!td_end(b->directory.directory,op,nonce))ok=false;
 if(!ok)fault(b);
 return ok;
}
enum nested_sparse_fill typed_root_registry_fill(struct typed_root_registry *b,
 struct nested_ept_context_item *item,uint64_t gpa,const struct nested_ept_result *mapped) {
 struct td_operation op;uint64_t nonce;lock(b);
 struct tr_registry_slot *s=refill_begin_locked(b,item,&op,&nonce);unlock(b);
 if(!s)return NESTED_SPARSE_INVALID;
 enum nested_sparse_fill status=nested_ept_sparse_fill(&item->tree,gpa,mapped);
 lock(b);bool ended=refill_end_locked(b,s,&op,nonce);unlock(b);
 return ended?status:NESTED_SPARSE_POISONED;
}
enum nested_sparse_lookup typed_root_registry_lookup(struct typed_root_registry *b,
 struct nested_ept_context_item *item,uint64_t gpa,uint8_t access,struct nested_ept_result *out) {
 struct td_operation op;uint64_t nonce;lock(b);
 struct tr_registry_slot *s=refill_begin_locked(b,item,&op,&nonce);unlock(b);
 if(!s)return NESTED_SPARSE_BAD_LOOKUP;
 enum nested_sparse_lookup status=nested_ept_sparse_lookup(&item->tree,gpa,access,out);
 lock(b);bool ended=refill_end_locked(b,s,&op,nonce);unlock(b);
 return ended?status:NESTED_SPARSE_LOOKUP_POISONED;
}
struct walk_context {
 tr_registry_resolve_fn resolve;
 void *context;
 struct tr_registry_walk snapshot;
};
static bool walk_resolve(void *context,uint64_t gpa,uint8_t need,uint64_t *hpa,
 const uint64_t **view,uint8_t *permissions) {
 struct walk_context *w=context;
 return w->resolve(w->context,&w->snapshot,gpa,need,hpa,view,permissions);
}
enum nested_sparse_fill typed_root_registry_miss(struct typed_root_registry *b,
 struct nested_ept_context_item *item,uint64_t gpa,uint8_t access,
 tr_registry_resolve_fn resolve,void *context,const struct nested_ept_policy *policy) {
 if(!resolve||!policy||!access||(access&~7))return NESTED_SPARSE_INVALID;
 struct td_operation op;uint64_t nonce;struct walk_context w={.resolve=resolve,.context=context};
 lock(b);struct tr_registry_slot *s=refill_begin_locked(b,item,&op,&nonce);
 if(s)w.snapshot=(struct tr_registry_walk){.virtual_key=item->key,
  .type_epoch=b->lease->type_epoch,.backing_hpa=b->backing_hpa,.type=b->lease->type};
 unlock(b);
 if(!s)return NESTED_SPARSE_INVALID;
 uint64_t page=gpa&~UINT64_C(4095);struct nested_ept_result result;
 enum nested_sparse_fill status=NESTED_SPARSE_INVALID;
 if(nested_ept_sparse_lookup(&item->tree,page,access,&result)==NESTED_SPARSE_MISS) {
  result=nested_ept_walk_memory(w.snapshot.virtual_key,page,1,walk_resolve,&w,policy);
  result=nested_sparse_walk_access(result,access);
  if(result.status==NESTED_EPT_OK)status=nested_ept_sparse_fill(&item->tree,page,&result);
 }
 lock(b);bool ended=refill_end_locked(b,s,&op,nonce);unlock(b);
 return ended?status:NESTED_SPARSE_POISONED;
}

/* Separate opt-in path: never changes the legacy resolver or its fault order. */
enum nested_sparse_fill typed_root_registry_miss_source(struct typed_root_registry *b,
 struct nested_ept_context_item *item,uint64_t gpa,uint8_t access,
 const struct tr_registry_source *source,void *context,struct nested_ept_result *out) {
 if(!out||!source||!source->guard||!source->translate||!access||(access&~7))return NESTED_SPARSE_INVALID;
 *out=(struct nested_ept_result){.status=NESTED_EPT_UNSUPPORTED};
 struct td_operation op;uint64_t nonce;struct tr_registry_walk snapshot={0};
 lock(b);struct tr_registry_slot *s=refill_begin_locked(b,item,&op,&nonce);
 if(s)snapshot=(struct tr_registry_walk){.virtual_key=item->key,
  .type_epoch=b->lease->type_epoch,.backing_hpa=b->backing_hpa,.type=b->lease->type};
 unlock(b);
 if(!s)return NESTED_SPARSE_INVALID;
 uint64_t page=gpa&~UINT64_C(4095);struct nested_ept_result cached;
 enum nested_sparse_fill status=NESTED_SPARSE_INVALID;
 if(source->guard(context,&snapshot)&&
    nested_ept_sparse_lookup(&item->tree,page,access,&cached)==NESTED_SPARSE_MISS) {
  *out=source->translate(context,&snapshot,page,access);
  if(!source->guard(context,&snapshot))out->status=NESTED_EPT_L0_BACKING_DENIED;
  if(out->status==NESTED_EPT_OK) {
   status=nested_ept_sparse_fill(&item->tree,page,out);
   if(!source->guard(context,&snapshot)) {
    lock(b);fault(b);unlock(b);out->status=NESTED_EPT_L0_BACKING_DENIED;
    status=NESTED_SPARSE_POISONED;
   }
  }
 }
 lock(b);bool ended=refill_end_locked(b,s,&op,nonce);unlock(b);
 return ended?status:NESTED_SPARSE_POISONED;
}
