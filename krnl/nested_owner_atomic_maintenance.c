#include "nested_owner_atomic_maintenance.h"
#include "nested_owner_sparse.h"
#define ADDRESS UINT64_C(0x000ffffffffff000)
bool noam_ticket_equal(struct tam_ticket a,struct tam_ticket b){
 return a.nonce==b.nonce&&a.stamp.domain_id==b.stamp.domain_id&&a.stamp.incarnation==b.stamp.incarnation&&
  a.stamp.context_id==b.stamp.context_id&&a.stamp.epoch==b.stamp.epoch&&a.stamp.lifetime==b.stamp.lifetime;
}
static bool fail(struct nested_owner_atomic_maintenance *r,enum noam_failure why,uint64_t observed){
 if(r->failure==NOAM_OK){r->failure=why;r->observed=observed;}
 r->phase=NOAM_UNCERTAIN;return false;
}
static bool local(struct nested_owner_atomic_maintenance *r){
 struct nested_owner_atomic *a=r?r->owner:0;
 if(!(a&&a->sparse&&a->vcpu&&r->maintenance&&a->atomic.maintenance==r->maintenance&&
  a->config.current_cpu&&a->config.current_cpu(a->config.context)==a->sparse->pool.owner&&
  a->vcpu->owner_apic==a->sparse->pool.owner&&a->vcpu->owner_apic<2&&
  a->physical_ad&&!a->failed&&a->sparse->pool.active&&!a->sparse->pool.fault&&
  !a->vcpu->busy&&!a->vcpu->poison&&(!a->healthy||a->healthy(a->healthy_context))))return false;
 /* Peer abort poisons both owners under the shared directory lock. */
 a->atomic.config.lock(a->atomic.config.context);
 bool ok=!a->atomic.poisoned;
 a->atomic.config.unlock(a->atomic.config.context);return ok;
}
static bool launch(struct nested_owner_atomic_maintenance *r){
 return r->local_launched01&&r->local_launched02&&*r->local_launched01==r->launched01&&
  *r->local_launched02==r->launched02&&r->owner->vcpu->launched01==r->vcpu_launched01&&
  r->owner->vcpu->launched02==r->vcpu_launched02&&r->owner->vcpu->vmcs01==r->vmcs01&&
  r->owner->vcpu->vmcs02==r->vmcs02;
}
bool noam_stopped(struct nested_owner_atomic_maintenance *r,struct tam_ticket t){
 return local(r)&&r->phase>=NOAM_BOUND&&r->phase<=NOAM_RESTORED&&r->failure==NOAM_OK&&
  noam_ticket_equal(r->ticket,t)&&launch(r)&&
  nested_domain_dispatch_maintenance_stopped(r->owner->dispatch,r->owner->vcpu->owner_apic,r->owner->token,r->domain_ticket);
}
bool noam_bind(struct nested_owner_atomic_maintenance *r,struct tam_ticket t,struct nested_domain_maintenance_ticket d){
 if(!local(r)||(r->phase!=NOAM_FRESH&&r->phase!=NOAM_COMMITTED)||!t.nonce||!t.stamp.epoch||!r->ops.current||!r->ops.load||
    !r->ops.read_eptp||!r->ops.write_eptp||!r->ops.invept||!r->ops.parking_ready||
    !r->local_launched01||!r->local_launched02||!r->composed_eptp)return false;
 bool reuse=r->phase==NOAM_COMMITTED;
 if(!reuse&&(r->ticket.nonce||r->committed_stamp.epoch||r->pending_item||r->failure!=NOAM_OK))return false;
 if(reuse){
  struct tam_ticket expected={r->committed_stamp,t.nonce};
  if(r->pending_item||r->failure!=NOAM_OK||!launch(r)||r->ticket.nonce==UINT64_MAX||
     t.nonce!=r->ticket.nonce+1||!noam_ticket_equal(t,expected)||
     d.domain_id!=r->domain_ticket.domain_id||d.incarnation!=r->domain_ticket.incarnation||
     d.purpose!=r->domain_ticket.purpose||r->domain_ticket.epoch==UINT64_MAX||
     d.epoch!=r->domain_ticket.epoch+1)return false;
 }
 /* Bootstrap config is deliberately not the post-prepare stamp authority. */
 struct tr_atomic_owner *a=&r->owner->atomic;
 a->config.lock(a->config.context);
 struct tam_ticket live={a->config.stamp,t.nonce};
 unsigned c=r->owner->vcpu->owner_apic;
 bool admitted=r->maintenance->state==TAM_STOPPING&&noam_ticket_equal(t,r->maintenance->ticket)&&
  !r->maintenance->operations[c].live&&!r->maintenance->operation_nonce[c]&&!r->maintenance->owner_busy[c];
 a->config.unlock(a->config.context);
 if(!admitted||!noam_ticket_equal(t,live)||!nested_domain_dispatch_maintenance_stopped(r->owner->dispatch,
    r->owner->vcpu->owner_apic,r->owner->token,d))return false;
 r->ticket=t;r->domain_ticket=d;
 if(!reuse){
  r->vmcs01=r->owner->vcpu->vmcs01;r->vmcs02=r->owner->vcpu->vmcs02;
  r->launched01=*r->local_launched01;r->launched02=*r->local_launched02;
  r->vcpu_launched01=r->owner->vcpu->launched01;r->vcpu_launched02=r->owner->vcpu->launched02;
 }
 r->saved_item=r->owner->sparse->hardware_item;
 r->parking_eptp=r->owner->sparse->fixture->eptp;
 if(!r->vmcs01||!r->vmcs02||r->vmcs01==r->vmcs02||!r->saved_item||
    r->saved_item!=r->owner->sparse->contexts.active||(r->parking_eptp&4095)!=0x1e)
  return fail(r,NOAM_GUARD,0);
 r->private_eptp=r->saved_item->tree.eptp;r->phase=NOAM_BOUND;
 uint64_t current=0,eptp=0;
 if(!r->ops.current(r->context,&current)||current!=r->vmcs02)return fail(r,NOAM_CURRENT,current);
 if(!r->ops.read_eptp(r->context,&eptp)||eptp!=r->private_eptp||*r->composed_eptp!=eptp)
  return fail(r,NOAM_READ02,eptp);
 return noam_stopped(r,t);
}
static bool identity(struct td_identity a,struct td_identity b){
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool root(struct nested_owner_atomic_maintenance *r,const struct tam_root *p,bool reset,unsigned pins){
 struct tr_atomic_owner *a=&r->owner->atomic;
 if(!p||!p->binding||!p->binding->slot||!p->item)return false;
 struct tr_atomic_binding *b=p->binding;struct td_record *d=&b->slot->directory.record;
 a->config.lock(a->config.context);
 bool ok=b->bound&&!b->released&&b->pin.live&&b->pin.nonce==b->pin_nonce&&b->pin_nonce&&
  b->pin.catalog==a->config.catalog&&b->pin.authority_context==a->config.authority_context&&
  p->item->transaction_token==&b->slot->directory&&identity(d->identity,p->identity)&&p->identity.owner==r->owner->vcpu->owner_apic&&
  d->state==TD_LIVE&&d->attached&&d->history==p->history&&!(p->history&~(UINT64_C(1)<<p->identity.owner))&&
  d->full_eptp==p->full_eptp&&d->hpa==p->hpa&&(p->full_eptp&ADDRESS)==p->hpa&&
  (p->full_eptp&4095)==0x5e&&p->item->tree.eptp==p->full_eptp&&!p->item->tree.poisoned&&
  p->item->tree.generation==(reset?p->new_generation:p->old_generation)&&
  d->cache_generation==(reset?p->new_generation:p->old_generation)&&
  p->new_generation==p->old_generation+1&&p->new_generation&&p->item->pins==pins&&d->pins==pins&&
  (!reset||!d->refills);
 a->config.unlock(a->config.context);
 return ok&&nrc_guard(&b->pin,b->pin_nonce);
}
static bool actual(struct nested_owner_atomic_maintenance *r,uint64_t expected,enum noam_failure why){
 uint64_t current=0,eptp=0;
 if(!r->ops.current(r->context,&current)||current!=r->vmcs02)return fail(r,NOAM_CURRENT,current);
 if(!r->ops.read_eptp(r->context,&eptp)||eptp!=expected)return fail(r,why,eptp);
 return true;
}
bool noam_remove(struct nested_owner_atomic_maintenance *r,struct tam_ticket t){
 if(!noam_stopped(r,t)||r->phase!=NOAM_BOUND)return false;
 struct nested_owner_sparse *s=r->owner->sparse;unsigned c=r->owner->vcpu->owner_apic;
 if(s->hardware_item!=r->saved_item||s->contexts.active!=r->saved_item||!r->ops.parking_ready(r->context))
  return fail(r,NOAM_GUARD,0);
 size_t count=r->maintenance->root_count[c];bool active=false;
 if(!count||count!=s->contexts.count)return fail(r,NOAM_GUARD,count);
 for(size_t n=0;n<count;n++){
  const struct tam_root *p=&r->maintenance->roots[c][n];bool selected=p->item==r->saved_item;
  if(!root(r,p,false,selected?1:0)||(p->full_eptp&ADDRESS)==(r->parking_eptp&ADDRESS))return fail(r,NOAM_GUARD,n);
  if(selected)active=true;
 }
 if(!active||!actual(r,r->private_eptp,NOAM_READ02))return fail(r,NOAM_GUARD,0);
 r->phase=NOAM_REMOVING;
 /* Verify the complete fixed physical reference set, not just current VMCS02. */
 if(!r->ops.load(r->context,r->vmcs01))return fail(r,NOAM_LOAD01,0);
 uint64_t eptp=0;
 if(!r->ops.read_eptp(r->context,&eptp)||eptp!=r->parking_eptp)return fail(r,NOAM_READ01,eptp);
 for(size_t n=0;n<count;n++)if((eptp&ADDRESS)==r->maintenance->roots[c][n].hpa)return fail(r,NOAM_READ01,eptp);
 if(!r->ops.load(r->context,r->vmcs02))return fail(r,NOAM_LOAD02,0);
 if(!actual(r,r->private_eptp,NOAM_READ02))return false;
 if(!r->ops.write_eptp(r->context,r->parking_eptp))return fail(r,NOAM_WRITE_PARK,0);
 if(!actual(r,r->parking_eptp,NOAM_VERIFY_PARK)||!noam_stopped(r,t))return fail(r,NOAM_GUARD,0);
 /* Metadata unpin is TAM's later operation, never performed by this adapter. */
 s->hardware_item=0;r->phase=NOAM_PARKED;return true;
}
bool noam_restore(struct nested_owner_atomic_maintenance *r,struct tam_ticket t,const struct tam_root *p){
 if(!noam_stopped(r,t)||r->phase!=NOAM_PARKED)return false;
 struct nested_owner_sparse *s=r->owner->sparse;
 if(!p||s->hardware_item||p->item!=s->contexts.active||p->item!=r->saved_item||
    p->full_eptp!=r->private_eptp||!root(r,p,true,1)||!r->ops.parking_ready(r->context))return fail(r,NOAM_GUARD,0);
 /* Durable before any possibly successful VMWRITE. Failure retains this and
  * TAM's prepared pin even if the instruction result/readback is uncertain. */
 r->pending_item=p->item;r->phase=NOAM_RESTORING;
 if(!actual(r,r->parking_eptp,NOAM_READ02))return false;
 if(!r->ops.write_eptp(r->context,p->full_eptp))return fail(r,NOAM_WRITE_RESTORE,0);
 if(!actual(r,p->full_eptp,NOAM_VERIFY_RESTORE)||!noam_stopped(r,t))return fail(r,NOAM_GUARD,0);
 s->hardware_item=p->item;*r->composed_eptp=p->full_eptp;r->phase=NOAM_RESTORED;return true;
}
bool noam_flush(struct nested_owner_atomic_maintenance *r,struct tam_ticket t,const struct tam_root *p,struct tam_flush_receipt *out){
 if(!out||!noam_stopped(r,t)||r->phase!=NOAM_PARKED)return false;
 if(r->owner->sparse->hardware_item||!root(r,p,false,0)||!actual(r,r->parking_eptp,NOAM_READ02))return fail(r,NOAM_GUARD,0);
 /* Root was zeroed by sparse reset; ordinary flush already occurred. This is
  * the separate exact hardware receipt before subordinate-page release. */
 struct td_record *d=&p->binding->slot->directory.record;
 r->owner->atomic.config.lock(r->owner->atomic.config.context);
 bool clean=!d->refills;
 r->owner->atomic.config.unlock(r->owner->atomic.config.context);
 if(!clean)return fail(r,NOAM_GUARD,0);
 if(!r->ops.invept(r->context,p->full_eptp))return fail(r,NOAM_INVEPT,p->full_eptp);
 if(!noam_stopped(r,t)||!root(r,p,false,0))return fail(r,NOAM_GUARD,0);
 unsigned c=r->owner->vcpu->owner_apic;
 *out=(struct tam_flush_receipt){t,p->identity,p->full_eptp,p->hpa,p->history,p->old_generation,p->new_generation,UINT64_C(1)<<c,c,true};
 return true;
}

bool noam_commit(struct nested_owner_atomic_maintenance *r,struct tam_ticket t,struct ad_entry_stamp published){
 if(!local(r)||r->phase!=NOAM_RESTORED||r->failure!=NOAM_OK||!noam_ticket_equal(r->ticket,t)||!launch(r))return false;
 struct tr_atomic_maintenance *m=r->maintenance;struct tr_atomic_owner *a=&r->owner->atomic;
 unsigned c=r->owner->vcpu->owner_apic;struct tam_root *active=0;
 a->config.lock(a->config.context);
 struct tam_ticket expected={m->next_stamp,t.nonce},outer={published,t.nonce},live={a->config.stamp,t.nonce};
 bool ok=m->state==TAM_OPEN&&noam_ticket_equal(m->ticket,t)&&noam_ticket_equal(expected,outer)&&
  noam_ticket_equal(expected,live)&&m->prepared==3&&m->reset==3&&m->revoked==3&&
  !m->owner_busy[c]&&!m->operations[c].live&&!m->operation_nonce[c]&&
  !a->fault.active&&!a->fault.admitted&&!a->fault.permit.active&&!a->entry_active&&!a->entry_operation.live&&
  !a->bridge->directory.busy&&!a->bridge->refill_operation&&
  r->pending_item&&r->pending_item==r->saved_item&&r->pending_item==r->owner->sparse->hardware_item&&
  r->pending_item==r->owner->sparse->contexts.active&&*r->composed_eptp==r->private_eptp;
 for(size_t n=0;ok&&n<m->root_count[c];n++)if(m->roots[c][n].item==r->pending_item)active=&m->roots[c][n];
 a->config.unlock(a->config.context);
 if(!ok||!active||!root(r,active,true,1)||!actual(r,r->private_eptp,NOAM_VERIFY_RESTORE)||
    !nested_domain_dispatch_consumer_stopped(r->owner->dispatch,c,r->owner->token))return fail(r,NOAM_GUARD,0);
 /* Hardware ownership remains represented by sparse.hardware_item and its
  * exact verified pin. Only this redundant in-flight handoff marker retires. */
 r->committed_stamp=published;r->pending_item=0;r->phase=NOAM_COMMITTED;return true;
}
