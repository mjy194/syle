#include "typed_root_local_provider.h"
#define ROOT_MASK UINT64_C(0x000ffffffffff000)
static bool identity_equal(struct td_identity a,struct td_identity b) {
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool ticket_equal(struct td_retire_ticket a,struct td_retire_ticket b) {
 return identity_equal(a.identity,b.identity)&&a.full_eptp==b.full_eptp&&a.hpa==b.hpa&&
  a.cache_generation==b.cache_generation&&a.history==b.history&&a.type_epoch==b.type_epoch&&a.nonce==b.nonce;
}
/* Every read of shared state, including slot membership and operation list,
 * is under the same short lock as the bridge. No global version equality. */
static bool pending(struct tr_local_provider *p,struct tr_registry_slot *s,
 struct tr_registry_retire_scope scope) {
 struct typed_root_registry *b=p->bridge;struct typed_root_lease *l=b->lease;
 struct typed_directory *d=b->directory.directory;struct td_retire_ticket t=scope.ticket;
 if(!l||!d||l->directory!=d||!b->root_scoped||b->directory.owner!=p->owner||
    !p->healthy(p->context)||p->current_cpu(p->context)!=p->owner||
    b->directory.faulted||l->poisoned||d->poisoned||d->closed||d->sealed||
    d->version==UINT64_MAX||l->version==UINT64_MAX||*p->serial==UINT64_MAX||
    l->type_epoch!=d->type_epoch||b->refill_operation||!b->directory.busy||
    !scope.operation_nonce||b->directory.nonce!=scope.operation_nonce||
    s->bridge!=b||s->directory.bridge!=&b->directory||s->directory.forgotten||
    !s->directory.reserved||s->directory.receipt.live||t.identity.owner!=p->owner||
    (t.history&~(UINT64_C(1)<<p->owner))||!ticket_equal(t,s->directory.retirement))return false;
 bool linked=false;for(struct tr_registry_slot *q=b->slots;q;q=q->next)if(q==s){linked=true;break;}
 if(!linked)return false;
 linked=false;for(struct td_registry_slot *q=b->directory.slots;q;q=q->next)
  if(q==&s->directory){linked=true;break;}
 if(!linked)return false;
 struct nested_ept_context_item *item=s->directory.item;struct trl_token token;
 return item&&!item->pins&&item->transaction_token==&s->directory&&
  item->key==s->directory.virtual_key&&item->tree.root&&item->tree.hardware_ad==b->atomic_wb&&
  l->atomic_wb==b->atomic_wb&&(!b->atomic_wb||(l->type==(l->atomic_uc?TRL_UC:TRL_WB)&&
   (item->key&4095)==0x5e&&(t.full_eptp&4095)==0x5e&&
   b->atomic_hooks.pending&&b->atomic_hooks.pending(b->atomic_context,s,item)))&&
  item->tree.eptp==t.full_eptp&&(t.full_eptp&ROOT_MASK)==t.hpa&&
  item->tree.generation==t.cache_generation&&s->typed.record==&s->directory.record&&
  s->typed.attached&&s->typed.full_eptp==t.full_eptp&&s->typed.hpa==t.hpa&&
  trl_token_read(l,&s->typed,&token)&&identity_equal(token.identity,t.identity)&&
  token.type_epoch==t.type_epoch&&
  td_retire_pending(d,&b->directory.operation,scope.operation_nonce,
                    &s->directory.record,s->directory.identity,t);
}
bool tr_local_provider_flush(struct tr_local_provider *p,struct tr_registry_slot *s,
 struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out) {
 if(!p||!p->bridge||!p->slots||!p->completed||!p->slot_count||p->owner>1||
    !p->serial||!p->lock||!p->unlock||!p->current_cpu||!p->healthy||!p->invalidate||!s||!out)return false;
 /* Compare pointers without subtracting a possibly foreign slot pointer. */
 size_t index=0;for(;index<p->slot_count;index++)if(s==&p->slots[index])break;
 if(index==p->slot_count||!p->lock(p->context))return false;
 bool ok=pending(p,s,scope);p->unlock(p->context);
 if(!ok)return false;
 struct tr_local_invalidation local={0};
 /* Required even for zero history: initialization/old-HPA reuse still needs
  * an actual physical EPTP invalidation. Never substitute the virtual key. */
 if(!p->invalidate(p->context,scope.ticket.full_eptp,&local))return false;
 if(!p->lock(p->context))return false;
 ok=pending(p,s,scope)&&local.success&&local.cpu==p->owner&&
    local.full_eptp==scope.ticket.full_eptp&&local.acked==(UINT64_C(1)<<p->owner);
 if(ok) {
  struct tr_local_completed done={.scope=scope,.invalidation=local,
   .acked=local.acked&scope.ticket.history,.flush_serial=++*p->serial,.complete=true};
  p->completed[index]=done;
  *out=(struct tr_registry_root_evidence){.scope=done.scope,
   .acked=done.acked,.flush_serial=done.flush_serial};
 }
 p->unlock(p->context);return ok;
}

bool tr_local_completed_matches(const struct tr_local_provider *p,const struct tr_registry_slot *s) {
 if(!p||!p->bridge||!p->slots||!p->completed||!s||p->owner>1)return false;
 size_t index=0;for(;index<p->slot_count;index++)if(s==&p->slots[index])break;
 if(index==p->slot_count)return false;
 const struct tr_local_completed *c=&p->completed[index];
 const struct td_receipt *r=&s->directory.receipt;
 return c->complete&&c->invalidation.success&&c->invalidation.cpu==p->owner&&
  c->invalidation.full_eptp==c->scope.ticket.full_eptp&&
  c->invalidation.acked==(UINT64_C(1)<<p->owner)&&
  !(c->scope.ticket.history&~c->invalidation.acked)&&c->acked==c->scope.ticket.history&&
  c->scope.ticket.identity.owner==p->owner&&c->scope.operation_nonce&&c->flush_serial&&
  s->bridge==p->bridge&&s->directory.bridge==&p->bridge->directory&&s->directory.forgotten&&
  !s->directory.item&&s->typed.state==TRL_RETIRED&&!s->typed.token_live&&
  s->typed.receipt==r&&s->typed.receipt_version==r->commit_version&&
  s->typed.receipt_flush_serial==c->flush_serial&&s->receipt_version==r->commit_version&&
  s->evidence_operation_nonce==c->scope.operation_nonce&&s->evidence_serial==c->flush_serial&&
  s->evidence_acked==c->acked&&ticket_equal(s->evidence_ticket,c->scope.ticket)&&
  ticket_equal(s->directory.retirement,c->scope.ticket)&&ticket_equal(s->typed.retirement,c->scope.ticket)&&
  td_receipt_matches(p->bridge->directory.directory,r,c->scope.ticket)&&
  r->acked==c->acked&&r->flush_serial==c->flush_serial;
}
