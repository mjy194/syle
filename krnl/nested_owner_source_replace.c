#include "nested_owner_source_replace.h"
#include "nested_owner_sparse.h"
#define ADDRESS UINT64_C(0x000ffffffffff000)
static bool source_identity(struct nrc_identity a,struct nrc_identity b){
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.catalog==b.catalog&&a.lifetime==b.lifetime;
}
static bool identity(struct td_identity a,struct td_identity b){
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool source_root(struct nrc_root a,struct nrc_root b){
 return source_identity(a.identity,b.identity)&&a.source==b.source&&a.owner==b.owner&&
  a.generation==b.generation&&a.lifetime==b.lifetime&&a.full_eptp==b.full_eptp&&a.root_gpa==b.root_gpa;
}
bool nosr_ticket_equal(struct tsr_ticket a,struct tsr_ticket b){
 return a.nonce&&a.nonce==b.nonce&&source_identity(a.source,b.source)&&
  a.stamp.domain_id==b.stamp.domain_id&&a.stamp.incarnation==b.stamp.incarnation&&
  a.stamp.context_id==b.stamp.context_id&&a.stamp.epoch==b.stamp.epoch&&a.stamp.lifetime==b.stamp.lifetime&&
  a.domain.domain_id==b.domain.domain_id&&a.domain.incarnation==b.domain.incarnation&&
  a.domain.epoch==b.domain.epoch&&a.domain.purpose==b.domain.purpose;
}
bool nosr_root_equal(struct tsr_root a,struct tsr_root b){
 return a.slot==b.slot&&identity(a.identity,b.identity)&&source_root(a.source,b.source)&&
  a.key==b.key&&a.eptp==b.eptp&&a.hpa==b.hpa&&a.generation==b.generation&&a.history==b.history&&a.pins==b.pins;
}
static bool fail(struct nested_owner_source_replace *r,enum nosr_failure why,uint64_t observed){
 if(r->failure==NOSR_OK){r->failure=why;r->observed=observed;}
 r->phase=NOSR_UNCERTAIN;return false;
}
bool nosr_init(struct nested_owner_source_replace *r,const struct nosr_config *c){
 if(!r||!c||r->phase!=NOSR_FRESH||!c->owner||!c->owner->vcpu||!c->owner->sparse||
  !c->replacement||!c->ops.current||!c->ops.load||!c->ops.read_eptp||!c->ops.write_eptp||
  !c->ops.parking_ready||!c->composed_eptp||!c->local_launched01||!c->local_launched02||
  !c->owner->config.lock||!c->owner->config.unlock||!c->owner->config.current_cpu||
  c->owner->vcpu->owner_apic>=TSR_OWNERS)return false;
 *r=(struct nested_owner_source_replace){.config=*c,.lock=c->owner->config.lock,
  .unlock=c->owner->config.unlock,.current_cpu=c->owner->config.current_cpu,
  .metadata_context=c->owner->config.context,.cpu=c->owner->vcpu->owner_apic,.phase=NOSR_READY};
 return true;
}
/* Synchronized phase is checked BEFORE mutable owner config/registry fields.
 * No other owner's local state is read. Runtime owns the stronger outer gate. */
static bool admitted(struct nested_owner_source_replace *r,struct tsr_ticket t,enum tsr_state state,bool busy){
 if(!r||r->phase==NOSR_FRESH||r->failure!=NOSR_OK||r->current_cpu(r->metadata_context)!=r->cpu)return false;
 struct tr_source_replace *m=r->config.replacement;struct tr_atomic_owner *a=&r->config.owner->atomic;
 r->lock(r->metadata_context);
 bool ok=m->state==state&&nosr_ticket_equal(m->ticket,t)&&m->owners[r->cpu]==a&&
  (!busy||m->busy[r->cpu])&&a->replacement==m&&!a->poisoned&&a->bridge&&
  !a->bridge->directory.faulted&&!a->bridge->lease->poisoned&&!a->bridge->directory.directory->poisoned;
 r->unlock(r->metadata_context);return ok;
}
static bool local(struct nested_owner_source_replace *r){
 struct nested_owner_atomic *a=r->config.owner;
 return a->physical_ad&&!a->failed&&a->vcpu->owner_apic==r->cpu&&
  a->sparse->pool.owner==r->cpu&&a->sparse->pool.active&&!a->sparse->pool.fault&&
  !a->vcpu->busy&&!a->vcpu->poison&&(!a->healthy||a->healthy(a->healthy_context))&&
  a->atomic.registry==&a->sparse->contexts&&a->atomic.config.lock==r->lock&&
  a->atomic.config.unlock==r->unlock&&a->atomic.config.context==r->metadata_context;
}
static bool launch(struct nested_owner_source_replace *r){
 struct nested_vcpu *v=r->config.owner->vcpu;
 return *r->config.local_launched01==r->launched01&&*r->config.local_launched02==r->launched02&&
  v->launched01==r->vcpu_launched01&&v->launched02==r->vcpu_launched02&&
  v->vmcs01==r->vmcs01&&v->vmcs02==r->vmcs02&&v->cleared01==r->cleared01&&v->cleared02==r->cleared02;
}
static bool stopped(struct nested_owner_source_replace *r,struct tsr_ticket t){
 return local(r)&&launch(r)&&nosr_ticket_equal(r->ticket,t)&&
  nested_domain_dispatch_source_stopped(r->config.owner->dispatch,r->cpu,r->config.owner->token,t.domain);
}
bool nosr_stopped(struct nested_owner_source_replace *r,struct tsr_ticket t){
 if(!r||r->phase<NOSR_BOUND||r->phase>NOSR_VALIDATED)return false;
 return (admitted(r,t,TSR_CLOSED,false)||admitted(r,t,TSR_PUBLISHED,false)||
  admitted(r,t,TSR_COMMITTING,false))&&stopped(r,t);
}
static bool actual(struct nested_owner_source_replace *r,uint64_t vmcs,uint64_t expected,enum nosr_failure why){
 uint64_t current=0,eptp=0;
 if(!r->config.ops.current(r->config.context,&current)||current!=vmcs)return fail(r,NOSR_CURRENT,current);
 if(!r->config.ops.read_eptp(r->config.context,&eptp)||eptp!=expected)return fail(r,why,eptp);
 return true;
}
static bool load_actual(struct nested_owner_source_replace *r,uint64_t vmcs,uint64_t expected,enum nosr_failure loading,enum nosr_failure reading){
 if(!r->config.ops.load(r->config.context,vmcs))return fail(r,loading,vmcs);
 return actual(r,vmcs,expected,reading);
}
bool nosr_bind(struct nested_owner_source_replace *r,struct tsr_ticket t){
 if(!r||r->phase!=NOSR_READY||!admitted(r,t,TSR_CLOSED,false)||!local(r)||
  t.domain.purpose!=NESTED_DOMAIN_SOURCE_REPLACE||!t.domain.epoch||!t.stamp.epoch||
  !nested_domain_dispatch_source_stopped(r->config.owner->dispatch,r->cpu,r->config.owner->token,t.domain))return false;
 struct nested_vcpu *v=r->config.owner->vcpu;struct nested_owner_sparse *s=r->config.owner->sparse;
 r->ticket=t;r->vmcs01=v->vmcs01;r->vmcs02=v->vmcs02;
 r->launched01=*r->config.local_launched01;r->launched02=*r->config.local_launched02;
 r->vcpu_launched01=v->launched01;r->vcpu_launched02=v->launched02;
 r->cleared01=v->cleared01;r->cleared02=v->cleared02;
 if(!r->vmcs01||!r->vmcs02||r->vmcs01==r->vmcs02||!s->fixture||
  !r->launched01||!r->launched02||!r->vcpu_launched01||!r->vcpu_launched02||r->cleared01||r->cleared02||
  !s->hardware_item||s->hardware_item!=s->contexts.active)return fail(r,NOSR_GUARD,0);
 r->parking_eptp=s->fixture->eptp;r->phase=NOSR_BOUND;
 if((r->parking_eptp&4095)!=0x1e||!r->config.ops.parking_ready(r->config.context))return fail(r,NOSR_GUARD,0);
 return actual(r,r->vmcs01,r->parking_eptp,NOSR_READ01)&&nosr_stopped(r,t);
}
/* Full stable identity first. Only then may the current slot item be touched.
 * Deliberately no saved old item pointer, including after allocator HPA reuse. */
static bool live_root(struct nested_owner_source_replace *r,const struct tsr_root *p,struct nested_ept_context_item **item){
 if(!p||!p->slot)return false;
 struct tr_atomic_owner *a=&r->config.owner->atomic;struct tr_atomic_binding *b=0;
 r->lock(r->metadata_context);
 for(size_t n=0;n<a->config.slot_count;n++)if(p->slot==&a->config.slots[n]){b=&a->config.bindings[n];break;}
 bool ok=b&&b->slot==p->slot&&b->bound&&!b->released&&b->pin.live&&b->pin_nonce&&b->pin.nonce==b->pin_nonce&&
  b->pin.catalog==a->config.catalog&&b->pin.authority_context==a->config.authority_context&&
  source_root(b->root,p->source)&&source_root(b->pin.root,p->source)&&source_identity(p->source.identity,a->config.catalog_identity)&&
  identity(p->slot->directory.identity,p->identity)&&identity(p->slot->directory.record.identity,p->identity)&&
  p->identity.owner==r->cpu&&p->slot->directory.reserved&&!p->slot->directory.forgotten&&
  p->slot->bridge==a->bridge&&p->slot->directory.bridge==&a->bridge->directory&&
  p->slot->typed.state==TRL_LIVE&&p->slot->typed.allocator_lifetime==p->identity.allocator_lifetime;
 struct nested_ept_context_item *i=ok?p->slot->directory.item:0;
 if(ok){
  struct td_record *d=&p->slot->directory.record;
  ok=i&&i->transaction_token==&p->slot->directory&&i->key==p->key&&p->slot->directory.virtual_key==p->key&&
   p->key==p->source.full_eptp&&i->tree.root&&i->tree.hardware_ad&&!i->tree.poisoned&&
   d->state==TD_LIVE&&d->attached&&d->full_eptp==p->eptp&&i->tree.eptp==p->eptp&&
   (p->eptp&4095)==0x5e&&(p->eptp&ADDRESS)==p->hpa&&d->hpa==p->hpa&&
   d->cache_generation==p->generation&&i->tree.generation==p->generation&&
   d->history==p->history&&!(p->history&~(UINT64_C(1)<<r->cpu))&&d->pins==p->pins&&i->pins==p->pins;
 }
 r->unlock(r->metadata_context);
 if(!ok||!nrc_guard(&b->pin,b->pin_nonce))return false;
 if(item)*item=i;
 return true;
}
bool nosr_park(struct nested_owner_source_replace *r,struct tsr_ticket t,const struct tsr_root *roots,size_t count,struct tsr_receipt *out){
 if(!r||!out||r->phase!=NOSR_BOUND||!admitted(r,t,TSR_CLOSED,true)||!stopped(r,t))return false;
 struct nested_owner_sparse *s=r->config.owner->sparse;struct tr_source_replace *m=r->config.replacement;
 if(!roots||!count||count>TSR_ROOTS||count!=m->count[r->cpu]||count!=s->contexts.count||
  !s->hardware_item||s->hardware_item!=s->contexts.active||!r->config.ops.parking_ready(r->config.context))return fail(r,NOSR_GUARD,count);
 bool found=false;
 for(size_t n=0;n<count;n++){
  struct nested_ept_context_item *i=0;
  if(!nosr_root_equal(roots[n],m->old[r->cpu][n])||!live_root(r,&roots[n],&i)||
   roots[n].hpa==(r->parking_eptp&ADDRESS))return fail(r,NOSR_GUARD,n);
  bool active=i==s->hardware_item;
  if(roots[n].pins!=(active?1u:0u)||(!active&&roots[n].key==m->active_key[r->cpu]))return fail(r,NOSR_GUARD,n);
  if(active){if(found||roots[n].key!=m->active_key[r->cpu])return fail(r,NOSR_GUARD,n);r->old=roots[n];found=true;}
 }
 if(!found||*r->config.composed_eptp!=r->old.eptp)return fail(r,NOSR_GUARD,0);
 r->phase=NOSR_PARKING;
 if(!actual(r,r->vmcs01,r->parking_eptp,NOSR_READ01)||
  !load_actual(r,r->vmcs02,r->old.eptp,NOSR_LOAD02,NOSR_READ02))return false;
 if(!r->config.ops.write_eptp(r->config.context,r->parking_eptp))return fail(r,NOSR_WRITE_PARK,0);
 if(!actual(r,r->vmcs02,r->parking_eptp,NOSR_VERIFY_PARK)||
  !load_actual(r,r->vmcs01,r->parking_eptp,NOSR_LOAD01,NOSR_READ01))return false;
 if(!admitted(r,t,TSR_CLOSED,true)||!stopped(r,t))return fail(r,NOSR_GUARD,0);
 /* Coordinator alone performs precise unpin/destroy after this receipt. */
 s->hardware_item=0;r->phase=NOSR_PARKED;*out=(struct tsr_receipt){t,r->old,r->cpu,true};return true;
}
bool nosr_restore(struct nested_owner_source_replace *r,struct tsr_ticket t,const struct tsr_root *p,struct tsr_receipt *out){
 if(!r||!out||r->phase!=NOSR_PARKED||!admitted(r,t,TSR_PUBLISHED,true)||!stopped(r,t))return false;
 struct nested_owner_sparse *s=r->config.owner->sparse;struct nested_ept_context_item *i=0;
 if(!p||!nosr_root_equal(*p,r->config.replacement->fresh[r->cpu])||p->pins!=1||p->key!=r->old.key||
  p->slot==r->old.slot||identity(p->identity,r->old.identity)||
  p->identity.allocator_lifetime==r->old.identity.allocator_lifetime||
  source_identity(p->source.identity,r->ticket.source)||p->hpa==(r->parking_eptp&ADDRESS)||
  s->hardware_item||!live_root(r,p,&i)||i!=s->contexts.active||r->pending_live||
  !r->config.ops.parking_ready(r->config.context))return fail(r,NOSR_GUARD,0);
 if(!actual(r,r->vmcs01,r->parking_eptp,NOSR_READ01))return false;
 /* Durable before any possibly successful VMWRITE. Failure retains both
  * this stable marker and the coordinator's already-acquired pin. */
 r->pending=*p;r->pending_live=true;r->phase=NOSR_RESTORING;
 if(!load_actual(r,r->vmcs02,r->parking_eptp,NOSR_LOAD02,NOSR_READ02))return false;
 if(!r->config.ops.write_eptp(r->config.context,p->eptp))return fail(r,NOSR_WRITE_RESTORE,0);
 if(!actual(r,r->vmcs02,p->eptp,NOSR_VERIFY_RESTORE)||
  !load_actual(r,r->vmcs01,r->parking_eptp,NOSR_LOAD01,NOSR_READ01))return false;
 if(!admitted(r,t,TSR_PUBLISHED,true)||!stopped(r,t)||!live_root(r,p,&i)||i!=s->contexts.active)return fail(r,NOSR_GUARD,0);
 s->hardware_item=i;*r->config.composed_eptp=p->eptp;r->phase=NOSR_RESTORED;
 *out=(struct tsr_receipt){t,*p,r->cpu,true};return true;
}
static bool restored(struct nested_owner_source_replace *r){
 struct nested_ept_context_item *i=0;struct nested_owner_sparse *s=r->config.owner->sparse;
 return r->pending_live&&r->pending.pins==1&&live_root(r,&r->pending,&i)&&
  i==s->hardware_item&&i==s->contexts.active&&*r->config.composed_eptp==r->pending.eptp;
}
static bool audit_restore(struct nested_owner_source_replace *r){
 return r->config.ops.parking_ready(r->config.context)&&
  actual(r,r->vmcs01,r->parking_eptp,NOSR_READ01)&&
  load_actual(r,r->vmcs02,r->pending.eptp,NOSR_LOAD02,NOSR_VERIFY_RESTORE)&&
  load_actual(r,r->vmcs01,r->parking_eptp,NOSR_LOAD01,NOSR_READ01)&&launch(r);
}
bool nosr_validate(struct nested_owner_source_replace *r,struct tsr_ticket t,struct tsr_receipt *out){
 if(!r||!out||r->phase!=NOSR_RESTORED||!admitted(r,t,TSR_COMMITTING,false)||!stopped(r,t))return false;
 struct tsr_receipt receipt=r->config.replacement->restore[r->cpu];
 if(!receipt.success||receipt.owner!=r->cpu||!nosr_ticket_equal(receipt.ticket,t)||
  !nosr_root_equal(receipt.root,r->pending)||!restored(r)||!audit_restore(r)||
  !admitted(r,t,TSR_COMMITTING,false)||!stopped(r,t))return fail(r,NOSR_GUARD,0);
 r->validation=receipt;r->phase=NOSR_VALIDATED;*out=receipt;return true;
}
bool nosr_commit(struct nested_owner_source_replace *r,struct tsr_ticket t){
 if(!r||r->phase!=NOSR_VALIDATED||!nosr_ticket_equal(r->ticket,t)||
  !admitted(r,t,TSR_RELEASED,false)||!tsr_released(r->config.replacement,t))return false;
 if(!local(r)||!launch(r)||!r->validation.success||r->validation.owner!=r->cpu||
  !nosr_ticket_equal(r->validation.ticket,t)||!nosr_root_equal(r->validation.root,r->pending)||
  !restored(r)||!audit_restore(r)||!nested_domain_dispatch_consumer_stopped(r->config.owner->dispatch,
   r->cpu,r->config.owner->token)||!tsr_released(r->config.replacement,t))return fail(r,NOSR_GUARD,0);
 /* Existing hardware pin is untouched. Never inspect old.slot->item here. */
 r->pending_live=false;r->pending=(struct tsr_root){0};r->phase=NOSR_COMMITTED;return true;
}

/* Both callers are parked by the chain before this audit. source_plan is a
 * read-only full-dispatch checkpoint check, not a close or a reserved ticket. */
bool nosr_checkpoint(struct nested_owner_source_replace *r,struct tr_source_replace *prior,
 struct tsr_ticket t,struct tsr_receipt *out){
 if(!r||!prior||!out||r->phase!=NOSR_READY||r->failure!=NOSR_OK||
    r->current_cpu(r->metadata_context)!=r->cpu)return false;
 struct nested_owner_atomic *a=r->config.owner;
 r->lock(r->metadata_context);
 bool bound=prior->state==TSR_RELEASED&&nosr_ticket_equal(prior->ticket,t)&&
  prior->owners[r->cpu]==&a->atomic&&a->atomic.replacement==prior;
 r->unlock(r->metadata_context);
 struct nested_domain_maintenance_ticket prospective;
 if(!bound||!local(r)||!nested_owner_atomic_stopped(a)||
    !nested_domain_dispatch_source_plan(a->dispatch,&prospective))return fail(r,NOSR_GUARD,0);
 struct nested_vcpu *v=a->vcpu;struct nested_owner_sparse *s=a->sparse;
 r->vmcs01=v->vmcs01;r->vmcs02=v->vmcs02;
 r->launched01=*r->config.local_launched01;r->launched02=*r->config.local_launched02;
 r->vcpu_launched01=v->launched01;r->vcpu_launched02=v->launched02;
 r->cleared01=v->cleared01;r->cleared02=v->cleared02;
 if(!r->vmcs01||!r->vmcs02||r->vmcs01==r->vmcs02||!s->fixture||
    !r->launched01||!r->launched02||!r->vcpu_launched01||!r->vcpu_launched02||r->cleared01||r->cleared02||
    !s->hardware_item||s->hardware_item!=s->contexts.active)return fail(r,NOSR_GUARD,0);
 struct tsr_root root={0};
 /* Compare the current item pointer only against stable known slot records;
  * live_root validates the full identity before following that item. */
 r->lock(r->metadata_context);
 for(size_t n=0;n<a->atomic.config.slot_count;n++){
  struct tr_registry_slot *slot=&a->atomic.config.slots[n];
  if(slot->directory.item!=s->hardware_item)continue;
  struct td_record *d=&slot->directory.record;
  root=(struct tsr_root){slot,slot->directory.identity,a->atomic.config.bindings[n].root,
   slot->directory.virtual_key,d->full_eptp,d->hpa,d->cache_generation,d->history,d->pins};break;
 }
 r->unlock(r->metadata_context);
 struct nested_ept_context_item *i=0;
 if(root.pins!=1||!live_root(r,&root,&i)||i!=s->hardware_item||*r->config.composed_eptp!=root.eptp)
  return fail(r,NOSR_GUARD,0);
 r->parking_eptp=s->fixture->eptp;
 if((r->parking_eptp&4095)!=0x1e||!r->config.ops.parking_ready(r->config.context)||
    !actual(r,r->vmcs01,r->parking_eptp,NOSR_READ01)||
    !load_actual(r,r->vmcs02,root.eptp,NOSR_LOAD02,NOSR_READ02)||
    !load_actual(r,r->vmcs01,r->parking_eptp,NOSR_LOAD01,NOSR_READ01))return fail(r,NOSR_GUARD,0);
 if(!local(r)||!launch(r)||!live_root(r,&root,&i)||i!=s->hardware_item||
    !nested_domain_dispatch_source_plan(a->dispatch,&prospective))return fail(r,NOSR_GUARD,0);
 *out=(struct tsr_receipt){t,root,r->cpu,true};return true;
}
