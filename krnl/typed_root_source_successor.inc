/* N48: a fresh successor, not reinitialization of the released coordinator.
 * All functions are internal held-metadata-lock operations. Physical/owner
 * observations are made by the controller before taking that lock. */
#include "nested_owner_source_replace.h"
static bool ss_disjoint(const void *a,size_t na,const void *b,size_t nb){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return a&&b&&na&&nb&&na<=UINTPTR_MAX-x&&nb<=UINTPTR_MAX-y&&(x+na<=y||y+nb<=x);
}
static bool ss_zero(const void *p,size_t n){
 const unsigned char *b=p;for(size_t i=0;i<n;i++)if(b[i])return false;return true;
}
bool tsr_successor_storage_locked(struct tr_source_replace *p,const void *x,size_t bytes){
 if(!p||!ss_disjoint(x,bytes,p,sizeof *p))return false;
 for(unsigned c=0;c<TSR_OWNERS;c++){
  struct tr_atomic_owner *o=p->owners[c];struct tr_local_provider *v=p->providers[c];
  if(!o||!v||!o->bridge||!ss_disjoint(x,bytes,o,sizeof *o)||
   !ss_disjoint(x,bytes,o->registry,sizeof *o->registry)||!ss_disjoint(x,bytes,o->bridge,sizeof *o->bridge)||
   !ss_disjoint(x,bytes,v,sizeof *v)||!ss_disjoint(x,bytes,o->bridge->lease,sizeof *o->bridge->lease)||
   !ss_disjoint(x,bytes,o->bridge->directory.directory,sizeof *o->bridge->directory.directory)||
   o->config.slot_count>SIZE_MAX/sizeof *o->config.slots||o->config.slot_count>SIZE_MAX/sizeof *o->config.bindings||
   o->config.trace_count>SIZE_MAX/sizeof *o->config.traces||v->slot_count>SIZE_MAX/sizeof *v->completed||
   !ss_disjoint(x,bytes,o->config.slots,o->config.slot_count*sizeof *o->config.slots)||
   !ss_disjoint(x,bytes,o->config.bindings,o->config.slot_count*sizeof *o->config.bindings)||
   !ss_disjoint(x,bytes,o->config.traces,o->config.trace_count*sizeof *o->config.traces)||
   !ss_disjoint(x,bytes,v->completed,v->slot_count*sizeof *v->completed)||
   (p->mirrors[c]&&!ss_disjoint(x,bytes,p->mirrors[c],sizeof *p->mirrors[c])))return false;
 }return true;
}
static bool ss_storage(struct tr_source_replace *n,struct tr_source_replace *p,struct tsr_successor_handoff *h,uint64_t *active){
 return ss_disjoint(n,sizeof *n,h,sizeof *h)&&ss_disjoint(n,sizeof *n,active,sizeof *active)&&
  ss_disjoint(h,sizeof *h,active,sizeof *active)&&tsr_successor_storage_locked(p,n,sizeof *n)&&
  tsr_successor_storage_locked(p,h,sizeof *h)&&tsr_successor_storage_locked(p,active,sizeof *active);
}
/* Minimum two-round control budget. Ordinary activity between rounds still
 * consumes the same finite counters; handoff and each close revalidate. */
static bool ss_two_round_budget(struct tr_source_replace *p){
 if(!sr_budget_locked(p)||p->next_nonce>UINT64_MAX-2||p->next_command>UINT64_MAX-128)return false;
 struct typed_directory *d=p->owners[0]->bridge->directory.directory;
 struct typed_root_lease *l=p->owners[0]->bridge->lease;
 if(UINT64_MAX-d->version<512||UINT64_MAX-d->next_id<4||UINT64_MAX-d->next_nonce<128||
    UINT64_MAX-l->version<128||UINT64_MAX-l->next_nonce<4||l->budget-l->reserved<4||
    UINT64_MAX-p->old_catalog->next_nonce<128||UINT64_MAX-p->old_gate->next_nonce<128)return false;
 for(size_t n=0;n<p->old_catalog->root_count;n++)if(p->old_catalog->roots[n].generation>UINT64_MAX-2)return false;
 for(unsigned c=0;c<2;c++){
  struct tr_atomic_owner *o=p->owners[c];size_t slots=0,traces=0;
  if(*p->providers[c]->serial>UINT64_MAX-4*TSR_ROOTS)return false;
  for(size_t n=0;n<o->config.slot_count;n++)if(!o->config.bindings[n].bound&&!o->config.bindings[n].pin.live&&!o->config.slots[n].directory.reserved)slots++;
  for(size_t n=0;n<o->config.trace_count;n++)if(!o->config.traces[n].used)traces++;
  if(slots<2||traces<2)return false;
 }return true;
}
bool tsr_bind_successor_locked(struct tsr_successor_handoff *h,struct tr_source_replace *p,
 struct tr_source_replace *n,struct nested_owner_source_replace *a,struct nested_owner_source_replace *b,
 uint64_t *active,uint64_t generation){
 if(!h||!p||!n||!a||!b||a==b||!active||!generation||generation==UINT64_MAX||
    p->state!=TSR_OPEN||p->successor||p->predecessor||p->generation||
    !ss_storage(n,p,h,active)||!ss_zero(h,sizeof *h)||!ss_zero(n,sizeof *n)||
    __atomic_load_n(active,__ATOMIC_ACQUIRE)||!sr_quiet(p->owners[0])||!sr_quiet(p->owners[1])||!ss_two_round_budget(p))return false;
 struct nested_owner_source_replace *refs[]={a,b};
 for(unsigned c=0;c<2;c++)if(refs[c]->phase!=NOSR_READY||refs[c]->failure!=NOSR_OK||
    refs[c]->cpu!=c||!refs[c]->config.owner||&refs[c]->config.owner->atomic!=p->owners[c]||
    refs[c]->config.replacement!=p||p->owners[c]->replacement!=p||
    !ss_disjoint(n,sizeof *n,refs[c],sizeof *refs[c])||!ss_disjoint(h,sizeof *h,refs[c],sizeof *refs[c]))return false;
 *h=(struct tsr_successor_handoff){.self=h,.prior=p,.next=n,.refs={a,b},.active_generation=active,
  .prior_generation=generation,.next_generation=generation+1};
 p->successor=h;p->generation=generation;
 __atomic_store_n(active,generation,__ATOMIC_RELEASE);return true;
}
static bool ss_bound(struct tsr_successor_handoff *h){
 return h&&h->self==h&&h->prior&&h->next&&h->prior->successor==h&&
  h->prior->generation==h->prior_generation&&h->prior_generation&&h->prior_generation!=UINT64_MAX&&
  h->next_generation==h->prior_generation+1&&h->active_generation&&
  __atomic_load_n(h->active_generation,__ATOMIC_ACQUIRE)==h->prior_generation&&!h->published;
}
bool tsr_departed_locked(struct tsr_successor_handoff *h,unsigned c,struct tsr_ticket t){
 if(!ss_bound(h)||c>=2||h->departed[c]||h->prior->state!=TSR_RELEASED||
    !sr_ticket_eq(h->prior->ticket,t)||(h->ticket.nonce&&!sr_ticket_eq(h->ticket,t)))return false;
 struct nested_owner_source_replace *r=h->refs[c];struct tr_source_replace *p=h->prior;
 if(!r||r->phase!=NOSR_COMMITTED||r->failure!=NOSR_OK||r->pending_live||r->pending.slot||
    r->config.replacement!=p||r->cpu!=c||p->owners[c]->replacement!=p||
    !sr_ticket_eq(r->ticket,t)||!sr_receipt(r->validation,p,c,p->fresh[c])||
    !shared_healthy_locked(p->owners[c]))return false;
 /* This is historical completion evidence. Its root need not remain live. */
 if(!h->ticket.nonce)h->ticket=t;
 h->departure[c]=r->validation;
 __atomic_store_n(&h->departed[c],h->prior_generation,__ATOMIC_RELEASE);return true;
}
static bool ss_current_root(struct tr_atomic_owner *o,const struct tsr_root *root){
 bool known=false;
 for(size_t n=0;n<o->config.slot_count;n++)if(root->slot==&o->config.slots[n]){known=true;break;}
 return known&&sr_live(o,root,true);
}
bool tsr_checkpoint_locked(struct tsr_successor_handoff *h,unsigned c,uint64_t generation,struct tsr_receipt receipt){
 if(!ss_bound(h)||c>=2||generation!=h->next_generation||h->arrived[c]||
    h->departed[0]!=h->prior_generation||h->departed[1]!=h->prior_generation||
    !sr_ticket_eq(h->ticket,h->prior->ticket)||!receipt.success||receipt.owner!=c||
    !sr_ticket_eq(receipt.ticket,h->ticket)||h->prior->state!=TSR_RELEASED)return false;
 struct tr_atomic_owner *o=h->prior->owners[c];
 if(o->replacement!=h->prior||!sr_quiet(o)||!ss_current_root(o,&receipt.root)||receipt.root.pins!=1||
    o->registry->active!=receipt.root.slot->directory.item)return false;
 h->checkpoint[c]=receipt;
 __atomic_store_n(&h->arrived[c],generation,__ATOMIC_RELEASE);return true;
}
static bool ss_current(struct tr_source_replace *p){
 if(p->state!=TSR_RELEASED||p->retired!=3||p->prepared!=3||p->busy[0]||p->busy[1]||
    !p->new_catalog||!p->new_gate||p->next_nonce==UINT64_MAX||!sr_budget_locked(p)||
    p->new_catalog->closed||p->new_catalog->superseded||p->new_gate->closed||p->new_gate->superseded||
    __atomic_load_n(&p->new_catalog->fault,__ATOMIC_ACQUIRE)||__atomic_load_n(&p->new_gate->fault,__ATOMIC_ACQUIRE)||
    UINT64_MAX-p->new_catalog->next_nonce<64||UINT64_MAX-p->new_gate->next_nonce<64||
    p->new_gate->references||p->new_gate->tokens||!tam_stamp_eq(p->new_gate->stamp,p->new_stamp))return false;
 for(size_t n=0;n<p->new_catalog->root_count;n++)if(p->new_catalog->roots[n].generation==UINT64_MAX)return false;
 for(unsigned c=0;c<2;c++){
  struct tr_atomic_owner *o=p->owners[c];struct tsr_command *q=&p->command[c];
  if(o->replacement!=p||!sr_quiet(o)||!sr_mirror(o,p->mirrors[c])||q->armed||q->consumed||q->admitted||q->nonce||
     o->config.catalog!=p->new_catalog||o->config.gate!=p->new_gate||
     !catalog_equal(o->config.catalog_identity,p->new_catalog->identity)||!tam_stamp_eq(o->config.stamp,p->new_stamp)||
     !o->registry->initialized||o->registry->poisoned||!o->registry->active||!o->registry->count||
     o->bridge->lease->type_epoch!=p->type_epoch)return false;
 }return true;
}
bool tsr_init_successor_locked(struct tsr_successor_handoff *h,const struct tsr_hooks *hooks,void *context){
 if(!ss_bound(h)||!hooks||!hooks->plan_close||!hooks->close||!hooks->stopped||!hooks->park||!hooks->restore||!hooks->complete||
    !ss_storage(h->next,h->prior,h,h->active_generation)||!ss_zero(h->next,sizeof *h->next)||
    !sr_ticket_eq(h->ticket,h->prior->ticket)||!ss_current(h->prior))return false;
 struct tr_source_replace *p=h->prior,*n=h->next;
 for(unsigned c=0;c<2;c++){
  struct nested_owner_source_replace *r=h->refs[c];
  if(h->departed[c]!=h->prior_generation||h->arrived[c]!=h->next_generation||
     !sr_receipt(h->departure[c],p,c,p->fresh[c])||!r||r->phase!=NOSR_COMMITTED||r->pending_live||r->pending.slot||
     r->failure!=NOSR_OK||!sr_ticket_eq(r->ticket,h->ticket)||!sr_receipt(r->validation,p,c,p->fresh[c])||
     !h->checkpoint[c].success||h->checkpoint[c].owner!=c||!sr_ticket_eq(h->checkpoint[c].ticket,h->ticket)||
     h->checkpoint[c].root.pins!=1||!ss_current_root(p->owners[c],&h->checkpoint[c].root)||
     p->owners[c]->registry->active!=h->checkpoint[c].root.slot->directory.item)return false;
 }
 /* No failure point after this: both bindings and active generation publish
  * together under the caller's shared metadata lock. No old evidence reset. */
 n->owners[0]=p->owners[0];n->owners[1]=p->owners[1];n->providers[0]=p->providers[0];n->providers[1]=p->providers[1];
 n->mirrors[0]=p->mirrors[0];n->mirrors[1]=p->mirrors[1];n->hooks=*hooks;n->context=context;
 n->old_catalog=p->new_catalog;n->old_gate=p->new_gate;n->type_epoch=p->type_epoch;
 n->next_nonce=p->next_nonce;n->next_command=p->next_command;n->predecessor=p;n->generation=h->next_generation;
 for(unsigned c=0;c<2;c++){
  n->registry_config[c]=*p->owners[c]->registry;
  n->registry_config[c].items=n->registry_config[c].active=NULL;n->registry_config[c].count=0;
 }
 n->state=TSR_OPEN;
 for(unsigned c=0;c<2;c++)n->owners[c]->replacement=n;
 h->published=true;__atomic_store_n(h->active_generation,h->next_generation,__ATOMIC_RELEASE);return true;
}
