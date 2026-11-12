#include "nested_ept_atomic_format.h"
/* Kept with the production atomic bridge so maintenance shares its exact
 * identity/typed/source checks rather than reimplementing weaker predicates. */
static bool tam_stamp_eq(struct ad_entry_stamp a,struct ad_entry_stamp b) {
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.context_id==b.context_id&&a.epoch==b.epoch&&a.lifetime==b.lifetime;
}
static bool tam_ticket_eq(struct tam_ticket a,struct tam_ticket b) {
 return a.nonce&&a.nonce==b.nonce&&tam_stamp_eq(a.stamp,b.stamp);
}
static void tam_lock(struct tr_atomic_maintenance *m){lock(m->owners[0]);}
static void tam_unlock(struct tr_atomic_maintenance *m){unlock(m->owners[0]);}
static bool tam_valid_locked(struct tr_atomic_maintenance *m,struct tam_ticket t) {
 return tam_ticket_eq(m->ticket,t)&&m->state!=TAM_OPEN&&m->state!=TAM_POISONED&&m->state!=TAM_UNINITIALIZED&&
  m->owners[0]->bridge->lease->type_epoch==m->type_epoch&&healthy_locked(m->owners[0])&&healthy_locked(m->owners[1]);
}
void tam_abort(struct tr_atomic_maintenance *m) {
 if(!m||!m->owners[0])return;
 tam_lock(m);m->state=TAM_POISONED;
 for(unsigned c=0;c<TAM_OWNERS;c++) {
  struct tr_atomic_owner *o=m->owners[c];o->poisoned=true;o->registry->poisoned=true;o->bridge->directory.faulted=true;
 }
 trl_poison(m->owners[0]->bridge->lease);tam_unlock(m);
 ad_updater_gate_poison(m->owners[0]->config.gate);
}
static bool tam_fail(struct tr_atomic_maintenance *m){tam_abort(m);return false;}
static bool tam_stopped(struct tr_atomic_maintenance *m,struct tam_ticket t) {
 for(unsigned c=0;c<TAM_OWNERS;c++)if(!m->hooks.stopped(m->context,c,t))return false;
 return true;
}
static bool tam_quiet_locked(struct tr_atomic_owner *o) {
 return healthy_locked(o)&&!o->fault.active&&!o->fault.admitted&&!o->fault.permit.active&&!o->entry_active&&
  !o->entry_operation.live&&!o->bridge->directory.busy&&!o->bridge->refill_operation;
}
bool tam_init(struct tr_atomic_maintenance *m,struct tr_atomic_owner *a,struct tr_atomic_owner *b,
 const struct tam_hooks *h,void *context) {
 if(!m||m->state!=TAM_UNINITIALIZED||!a||!b||a==b||!a->initialized||!b->initialized||a->bridge->directory.directory->records||a->bridge->directory.directory->operations||a->maintenance||b->maintenance||a->replacement||b->replacement||
    !h||!h->close_entry||!h->stopped||!h->remove_refs||!h->flush||!h->restore_refs||!h->open_entry||!!h->load!=!!h->cas||
    a->registry->count||b->registry->count||a->registry->items||b->registry->items||
    a->registry->tick||b->registry->tick||a->bridge->slots||b->bridge->slots||
    !tam_quiet_locked(a)||!tam_quiet_locked(b)||
    a->bridge->directory.owner!=0||b->bridge->directory.owner!=1||
    a->bridge->lease!=b->bridge->lease||a->bridge->directory.directory!=b->bridge->directory.directory||
    a->bridge->directory.directory->participants!=3||a->config.catalog!=b->config.catalog||a->config.gate!=b->config.gate||
    !a->config.catalog->root_count||a->config.catalog->root_count>TAM_PAGES||
    !tam_stamp_eq(a->config.stamp,b->config.stamp)||!catalog_equal(a->config.catalog_identity,b->config.catalog_identity)||
    a->config.lock!=b->config.lock||a->config.unlock!=b->config.unlock||!healthy_locked(a)||!healthy_locked(b))return false;
 /* This large object must not lower to a libc memset in the freestanding
  * kernel. Match nested_parallel_init's explicit volatile byte stores. */
 struct tam_hooks hooks=*h;uint64_t type_epoch=a->bridge->lease->type_epoch;
 volatile unsigned char *bytes=(void *)m;
 for(size_t n=0;n<sizeof *m;n++)bytes[n]=0;
 m->owners[0]=a;m->owners[1]=b;m->hooks=hooks;m->context=context;
 m->state=TAM_OPEN;m->type_epoch=type_epoch;
 m->source_count=a->config.catalog->root_count;
 for(size_t n=0;n<m->source_count;n++)m->sources[n]=a->config.catalog->roots[n];
 a->maintenance=m;b->maintenance=m;return true;
}
bool tam_close(struct tr_atomic_maintenance *m,struct tam_ticket *out) {
 if(!m||!out||!m->owners[0])return false;
 tam_lock(m);
 bool ok=m->state==TAM_OPEN&&m->next_nonce!=UINT64_MAX&&healthy_locked(m->owners[0])&&healthy_locked(m->owners[1]);
 if(ok) {
  m->ticket=(struct tam_ticket){m->owners[0]->config.stamp,++m->next_nonce};
  m->revoked=m->reset=m->prepared=m->changed=m->cleared_entries=0;m->page_count=0;m->drain_polls=0;
  for(unsigned c=0;c<TAM_OWNERS;c++){m->root_count[c]=0;m->owner_busy[c]=false;
   for(unsigned n=0;n<TAM_ROOTS;n++)m->receipts[c][n]=(struct tam_flush_receipt){0};}
  /* Reserve this round before the unlocked physical admission callback. */
  m->state=TAM_STOPPING;*out=m->ticket;
 }
 tam_unlock(m);if(!ok)return false;
 if(!m->hooks.close_entry(m->context,m->ticket)||!tam_stopped(m,m->ticket))return tam_fail(m);
 tam_lock(m);ok=m->state==TAM_STOPPING;if(ok)m->state=TAM_CLOSING;tam_unlock(m);
 return ok?true:tam_fail(m);
}
enum tam_drain_result tam_drain(struct tr_atomic_maintenance *m,struct tam_ticket t) {
 if(!m||!m->owners[0])return TAM_DRAIN_FAILED;
 tam_lock(m);bool valid=tam_valid_locked(m,t)&&(m->state==TAM_CLOSING||m->state==TAM_UPDATER_DRAIN);
 bool close=false,wait=false,bad=false;
 if(valid) {
  if(++m->drain_polls>TAM_DRAIN_POLLS)bad=true;
  else if(m->state==TAM_CLOSING) {
   struct typed_directory *d=m->owners[0]->bridge->directory.directory;
   wait=d->operations||d->ops;
   if(!wait) {
    if(!tam_quiet_locked(m->owners[0])||!tam_quiet_locked(m->owners[1]))bad=true;
    else {m->state=TAM_UPDATER_DRAIN;close=true;}
   }
  }
 }
 tam_unlock(m);if(!valid)return TAM_DRAIN_FAILED;
 if(bad||!tam_stopped(m,t)){tam_abort(m);return TAM_DRAIN_FAILED;}
 if(wait)return TAM_DRAIN_WAIT;
 struct ad_updater_gate *g=m->owners[0]->config.gate;
 if(close&&!ad_updater_gate_close(g,t.stamp)){tam_abort(m);return TAM_DRAIN_FAILED;}
 if(!ad_updater_gate_drained(g,t.stamp)) {
  if(__atomic_load_n(&g->fault,__ATOMIC_ACQUIRE)){tam_abort(m);return TAM_DRAIN_FAILED;}
  return TAM_DRAIN_WAIT;
 }
 tam_lock(m);valid=tam_valid_locked(m,t)&&m->state==TAM_UPDATER_DRAIN;
 if(valid)m->state=TAM_DRAINED;
 tam_unlock(m);return valid?TAM_DRAIN_READY:TAM_DRAIN_FAILED;
}
static bool tam_owner_start(struct tr_atomic_maintenance *m,unsigned c,struct tam_ticket t,enum tam_state state) {
 if(!m||c>=TAM_OWNERS||!m->owners[0])return false;
 struct tr_atomic_owner *o=m->owners[c];
 if(o->config.current_cpu(o->config.context)!=c)return false;
 tam_lock(m);uint64_t done=state==TAM_DRAINED?m->revoked:state==TAM_CLEARED?m->reset:m->prepared;
 bool ok=tam_valid_locked(m,t)&&m->state==state&&!(done&(UINT64_C(1)<<c))&&!m->owner_busy[c];
 if(ok)m->owner_busy[c]=true;
 tam_unlock(m);return ok;
}
static bool tam_operation_start(struct tr_atomic_maintenance *m,unsigned c,struct tam_ticket t) {
 if(!m->hooks.stopped(m->context,c,t)||!ad_updater_gate_drained(m->owners[c]->config.gate,t.stamp))return false;
 tam_lock(m);struct tr_atomic_owner *o=m->owners[c];
 bool ok=tam_valid_locked(m,t)&&tam_quiet_locked(o)&&!m->operations[c].live&&
  td_begin(o->bridge->directory.directory,&m->operations[c],&m->operation_nonce[c]);
 tam_unlock(m);return ok;
}
static bool tam_operation_end(struct tr_atomic_maintenance *m,unsigned c) {
 tam_lock(m);bool ok=td_end(m->owners[c]->bridge->directory.directory,&m->operations[c],m->operation_nonce[c]);
 if(ok)m->operation_nonce[c]=0;
 tam_unlock(m);return ok;
}
static bool tam_root_locked(struct tr_atomic_maintenance *m,unsigned c,const struct tam_root *r,bool reset) {
 struct tr_atomic_owner *o=m->owners[c];struct tr_atomic_binding *a=r->binding;
 return a&&pin_identity(o,a)&&root_equal(a->root,r->source)&&slot_locked(o,a,r->item)&&
  identity_equal(a->slot->directory.identity,r->identity)&&a->slot->directory.record.history==r->history&&
  r->item->tree.eptp==r->full_eptp&&a->slot->directory.record.hpa==r->hpa&&
  r->item->tree.generation==(reset?r->new_generation:r->old_generation)&&
  a->slot->directory.record.state==TD_LIVE&&a->slot->typed.state==TRL_LIVE&&a->slot->typed.token_live;
}
bool tam_revoke(struct tr_atomic_maintenance *m,unsigned c,struct tam_ticket t) {
 if(!tam_owner_start(m,c,t,TAM_DRAINED))return false;
 struct tr_atomic_owner *o=m->owners[c];
 if(!tam_operation_start(m,c,t))return tam_fail(m);
 /* No page mutation before complete owner inventory validation. */
 tam_lock(m);bool ok=true;size_t count=0;
 for(struct nested_ept_context_item *item=o->registry->items;item&&ok;item=item->next) {
  struct tr_atomic_binding *a=binding(o,item->transaction_token);
  ok=count<TAM_ROOTS&&pin_identity(o,a)&&slot_locked(o,a,item)&&!item->tree.poisoned&&
   item->tree.generation!=UINT64_MAX&&item->pins==a->slot->directory.record.pins&&
   a->slot->directory.record.state==TD_LIVE&&a->slot->typed.state==TRL_LIVE&&a->slot->typed.token_live&&
   !(a->slot->directory.record.history&~(UINT64_C(1)<<c));
  if(ok)m->roots[c][count++]=(struct tam_root){a,item,a->slot->directory.identity,a->root,
   item->tree.eptp,a->slot->directory.record.hpa,a->slot->directory.record.history,item->tree.generation,item->tree.generation+1};
 }
 if(ok)ok=count&&count==o->registry->count&&o->registry->active;
 /* Every attached directory record must occur in the owner's context list. */
 for(struct tr_registry_slot *s=o->bridge->slots;s&&ok;s=s->next)if(!s->directory.forgotten) {
  bool found=false;for(size_t n=0;n<count;n++)if(m->roots[c][n].binding->slot==s)found=true;
  if(!found)ok=false;
 }
 if(ok)m->root_count[c]=count;
 tam_unlock(m);if(!ok)return tam_fail(m);
 for(size_t n=0;n<count;n++)if(!source_guard(o,m->roots[c][n].binding))return tam_fail(m);
 if(!m->hooks.remove_refs(m->context,c,t)||!m->hooks.stopped(m->context,c,t))return tam_fail(m);
 tam_lock(m);
 for(size_t n=0;n<count&&ok;n++) {
  struct tam_root *r=&m->roots[c][n];struct td_record *record=&r->binding->slot->directory.record;
  ok=tam_root_locked(m,c,r,false);
  while(ok&&r->item->pins) {
   ok=td_unpin(o->bridge->directory.directory,&m->operations[c],m->operation_nonce[c],record,r->identity);
   if(ok)r->item->pins--;
  }
  for(size_t k=0;k<o->config.trace_count&&ok;k++) {
   struct tr_atomic_trace *trace=&o->config.traces[k];
   if(!trace->used||trace->binding!=r->binding)continue;
   trace->usable=false;
   if(trace->registered) {
    ok=td_refill_drop(o->bridge->directory.directory,&m->operations[c],m->operation_nonce[c],record,r->identity);
    if(ok)trace->registered=false;
   }
  }
  if(ok)ok=!record->pins&&!r->item->pins&&!record->refills;
 }
 tam_unlock(m);
 if(!ok||!tam_operation_end(m,c))return tam_fail(m);
 tam_lock(m);m->revoked|=UINT64_C(1)<<c;m->owner_busy[c]=false;tam_unlock(m);return true;
}
static uint64_t tam_load(struct tr_atomic_maintenance *m,uint64_t *p) {
 return m->hooks.load?m->hooks.load(m->context,p):__atomic_load_n(p,__ATOMIC_ACQUIRE);
}
static bool tam_cas(struct tr_atomic_maintenance *m,uint64_t *p,uint64_t a,uint64_t b) {
 return m->hooks.cas?m->hooks.cas(m->context,p,a,b):__atomic_compare_exchange_n(p,&a,b,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static struct tam_page *tam_page_find(struct tr_atomic_maintenance *m,uint64_t gpa) {
 for(size_t n=0;n<m->page_count;n++)if(m->pages[n].gpa==gpa)return &m->pages[n];
 return NULL;
}
static bool tam_resolve(struct tr_atomic_maintenance *m,uint64_t gpa,struct nrc_atomic_page *out) {
 struct tr_atomic_owner *o=m->owners[0];struct tr_atomic_binding *a=m->roots[0][0].binding;
 return source_guard(o,a)&&nrc_resolve_atomic(&a->pin,a->pin_nonce,gpa,1,out);
}
static bool tam_graph(struct tr_atomic_maintenance *m,uint64_t gpa,unsigned depth) {
 struct tam_page *p=tam_page_find(m,gpa);
 if(!p||depth>3)return false;
 if(p->visited)return p->depth==depth;
 p->visited=true;p->depth=depth;
 uint64_t mask=((UINT64_C(1)<<m->owners[0]->config.catalog->maxphys)-1)&~UINT64_C(4095);
 for(unsigned n=0;n<512;n++) {
  uint64_t raw=p->raw[n];if(!(raw&7))continue;
  unsigned shift;
  if(!nea_decode_policy(raw,depth,mask,true,m->owners[0]->config.catalog->atomic_2m,m->owners[0]->config.catalog->atomic_1g,m->owners[0]->config.catalog->atomic_uc,&shift))return false;
  if(!shift) {if(!tam_graph(m,raw&mask,depth+1))return false;}
  else if(shift==12) {struct nrc_atomic_page leaf;
   if(!tam_resolve(m,raw&mask,&leaf)||leaf.backing.memory_type!=((raw>>3)&7)||
      (leaf.backing.memory_type==0&&(leaf.backing.permissions&4)))return false;}
  /* A 2MiB/1GiB source leaf does not authorize all its backing pages. Sparse
   * offsets, including holes, are resolved per 4KiB at fault time. Clearing
   * the source A/D word requires only this pinned table's write authority. */
 }
 return true;
}
static bool tam_clear_guard(struct tr_atomic_maintenance *m,struct tam_ticket t) {
 tam_lock(m);struct typed_directory *d=m->owners[0]->bridge->directory.directory;
 bool ok=tam_valid_locked(m,t)&&m->state==TAM_CLEARING&&m->revoked==3&&!d->operations&&!d->ops;
 for(unsigned c=0;c<TAM_OWNERS&&ok;c++)for(size_t n=0;n<m->root_count[c]&&ok;n++) {
  struct tam_root *r=&m->roots[c][n];ok=tam_root_locked(m,c,r,false)&&!r->item->pins&&
   !r->binding->slot->directory.record.pins&&!r->binding->slot->directory.record.refills;
 }
 for(struct td_record *record=d->records;record&&ok;record=record->next) {
  bool found=false;for(unsigned c=0;c<TAM_OWNERS;c++)for(size_t n=0;n<m->root_count[c];n++)
   if(record==&m->roots[c][n].binding->slot->directory.record)found=true;
  if(!found)ok=false;
 }
 tam_unlock(m);
 return ok&&tam_stopped(m,t)&&ad_updater_gate_drained(m->owners[0]->config.gate,t.stamp);
}
/* Full final authority and word audit catches late failure after a last CAS.
 * Structure is immutable under the retained source pins; present words must
 * be clean and absent payload must remain exactly unchanged. */
static bool tam_sources_valid(struct tr_atomic_maintenance *m) {
 struct tr_atomic_owner *owner=m->owners[0];struct nested_root_catalog *catalog=owner->config.catalog;
 if(catalog->root_count!=m->source_count)return false;
 for(size_t n=0;n<m->source_count;n++) {
  struct nrc_root current;
  if(!nrc_find(catalog,owner->config.catalog_identity,m->sources[n].full_eptp,&current)||!root_equal(current,m->sources[n]))return false;
 }
 for(unsigned c=0;c<TAM_OWNERS;c++)for(size_t n=0;n<m->root_count[c];n++)
  if(!source_guard(m->owners[c],m->roots[c][n].binding))return false;
 return true;
}
static bool tam_source_clean(struct tr_atomic_maintenance *m) {
 if(!tam_sources_valid(m))return false;
 for(size_t k=0;k<m->page_count;k++) {
  struct tam_page *p=&m->pages[k];struct nrc_atomic_page now;
  if(!tam_resolve(m,p->gpa,&now)||now.backing.hpa!=p->hpa||now.atomic_view!=p->view)return false;
  for(unsigned n=0;n<512;n++) {
   uint64_t mask=(p->raw[n]&7)?((p->depth==3||((p->depth==1||p->depth==2)&&(p->raw[n]&0x80)))?0x300:0x100):0;
   if(tam_load(m,&p->view[n])!=(p->raw[n]&~mask))return false;
  }
 }
 return true;
}
bool tam_clear(struct tr_atomic_maintenance *m,struct tam_ticket t) {
 if(!m||!m->owners[0])return false;
 tam_lock(m);bool ok=tam_valid_locked(m,t)&&m->state==TAM_DRAINED&&m->revoked==3&&!m->owner_busy[0]&&!m->owner_busy[1];
 if(ok)m->state=TAM_CLEARING;
 tam_unlock(m);if(!ok)return false;
 struct nested_root_catalog *catalog=m->owners[0]->config.catalog;
 if(!tam_sources_valid(m))return tam_fail(m);
 /* Unique registered GPA/HPA spans give a deduplicated inventory. Traverse
  * all roots to assign levels; reject unreferenced/inconsistent-level pages. */
 for(size_t s=0;s<catalog->span_count;s++)if(catalog->spans[s].role==NRC_TABLE) {
  const struct nrc_span *span=&catalog->spans[s];
  for(uint64_t off=0;off<span->bytes;off+=4096) {
   struct nrc_atomic_page page;
   if(m->page_count==TAM_PAGES||!tam_clear_guard(m,t)||!tam_resolve(m,span->gpa+off,&page))return tam_fail(m);
   struct tam_page *p=&m->pages[m->page_count++];
   *p=(struct tam_page){.gpa=span->gpa+off,.hpa=page.backing.hpa,.view=page.atomic_view};
   if(!p->view)return tam_fail(m);
   for(unsigned n=0;n<512;n++)p->raw[n]=tam_load(m,&p->view[n]);
  }
 }
 for(size_t r=0;r<catalog->root_count;r++)if(!tam_graph(m,catalog->roots[r].root_gpa,0))return tam_fail(m);
 for(size_t p=0;p<m->page_count;p++)if(!m->pages[p].visited)return tam_fail(m);
 for(size_t k=0;k<m->page_count;k++) {
  struct tam_page *p=&m->pages[k];struct nrc_atomic_page now;
  if(!tam_clear_guard(m,t)||!tam_resolve(m,p->gpa,&now)||now.backing.hpa!=p->hpa||now.atomic_view!=p->view)return tam_fail(m);
  for(unsigned n=0;n<512;n++) {
   uint64_t old=p->raw[n];if(!(old&7))continue;
   uint64_t mutable=(p->depth==3||((p->depth==1||p->depth==2)&&(p->raw[n]&0x80)))?0x300:0x100;bool done=false;
   for(unsigned attempt=0;attempt<64;attempt++) {
    if(!tam_clear_guard(m,t))return tam_fail(m);
    uint64_t raw=tam_load(m,&p->view[n]);
    if((raw&~mutable)!=(old&~mutable))return tam_fail(m);
    if(!(raw&mutable)){done=true;break;}
    if(tam_cas(m,&p->view[n],raw,raw&~mutable)) {
     m->changed++;done=true;
     if(!tam_clear_guard(m,t))return tam_fail(m);
     break;
    }
   }
   if(!done)return tam_fail(m);
   m->cleared_entries++;
  }
 }
 if(!tam_source_clean(m)||!tam_clear_guard(m,t))return tam_fail(m);
 tam_lock(m);m->state=TAM_CLEARED;tam_unlock(m);return true;
}
struct tam_reset_call {struct tr_atomic_maintenance *m;unsigned cpu;struct tam_ticket ticket;struct tam_root *root;};
static bool tam_receipt(void *context,const struct nested_ept_sparse *tree) {
 struct tam_reset_call *call=context;struct tr_atomic_maintenance *m=call->m;struct tam_root *r=call->root;
 if(tree!=&r->item->tree||tree->generation!=r->old_generation||tree->eptp!=r->full_eptp)return false;
 struct tam_flush_receipt got={0};
 if(!m->hooks.flush(m->context,call->cpu,call->ticket,r,&got))return false;
 bool ok=got.success&&tam_ticket_eq(got.ticket,call->ticket)&&identity_equal(got.identity,r->identity)&&
  got.full_eptp==r->full_eptp&&got.hpa==r->hpa&&got.history==r->history&&
  got.old_generation==r->old_generation&&got.new_generation==r->new_generation&&
  got.cpu==call->cpu&&got.acked==(UINT64_C(1)<<call->cpu)&&!(r->history&~got.acked);
 tam_lock(m);ok=ok&&tam_valid_locked(m,call->ticket)&&m->state==TAM_CLEARED&&tam_root_locked(m,call->cpu,r,false)&&
  !r->item->pins&&!r->binding->slot->directory.record.pins&&!r->binding->slot->directory.record.refills;
 tam_unlock(m);
 ok=ok&&m->hooks.stopped(m->context,call->cpu,call->ticket)&&ad_updater_gate_drained(m->owners[0]->config.gate,call->ticket.stamp);
 if(ok){tam_lock(m);m->receipts[call->cpu][r-m->roots[call->cpu]]=got;tam_unlock(m);}
 return ok;
}
bool tam_reset(struct tr_atomic_maintenance *m,unsigned c,struct tam_ticket t) {
 if(!tam_owner_start(m,c,t,TAM_CLEARED))return false;
 if(!tam_operation_start(m,c,t))return tam_fail(m);
 struct tr_atomic_owner *o=m->owners[c];bool ok=true;
 for(size_t n=0;n<m->root_count[c]&&ok;n++) {
  struct tam_root *r=&m->roots[c][n];
  tam_lock(m);ok=tam_root_locked(m,c,r,false)&&!r->item->pins&&!r->binding->slot->directory.record.refills;tam_unlock(m);
  if(!ok||!source_guard(o,r->binding))return tam_fail(m);
  struct tam_reset_call call={m,c,t,r};
  if(!nested_ept_sparse_reset_guarded(&r->item->tree,tam_receipt,&call))return tam_fail(m);
  tam_lock(m);ok=td_cache_reset(o->bridge->directory.directory,&m->operations[c],m->operation_nonce[c],
   &r->binding->slot->directory.record,r->identity,r->new_generation)&&tam_root_locked(m,c,r,true);tam_unlock(m);
 }
 if(!ok||!tam_operation_end(m,c))return tam_fail(m);
 tam_lock(m);m->reset|=UINT64_C(1)<<c;m->owner_busy[c]=false;
 if(m->reset==3){m->state=TAM_PREPARING;m->next_stamp=t.stamp;m->next_stamp.epoch++;}
 bool overflow=m->reset==3&&!m->next_stamp.epoch;tam_unlock(m);
 if(overflow)return tam_fail(m);
 return true;
}
bool tam_prepare(struct tr_atomic_maintenance *m,unsigned c,struct tam_ticket t) {
 if(!tam_owner_start(m,c,t,TAM_PREPARING))return false;
 if(!tam_operation_start(m,c,t))return tam_fail(m);
 struct tr_atomic_owner *o=m->owners[c];struct tam_root *active=NULL;bool ok=true;
 tam_lock(m);
 for(size_t n=0;n<m->root_count[c]&&ok;n++) {
  struct tam_root *r=&m->roots[c][n];ok=tam_root_locked(m,c,r,true)&&!r->item->pins&&!r->binding->slot->directory.record.refills;
  if(r->item==o->registry->active)active=r;
 }
 if(ok)ok=active&&td_pin(o->bridge->directory.directory,&m->operations[c],m->operation_nonce[c],
  &active->binding->slot->directory.record,active->identity);
 if(ok){active->item->pins++;o->config.stamp=m->next_stamp;}
 tam_unlock(m);
 if(!ok||!source_guard(o,active->binding)||!m->hooks.restore_refs(m->context,c,t,active)||
    !m->hooks.stopped(m->context,c,t)||!tam_operation_end(m,c))return tam_fail(m);
 tam_lock(m);ok=tam_valid_locked(m,t)&&tam_root_locked(m,c,active,true)&&active->item->pins==1&&active->binding->slot->directory.record.pins==1;
 if(ok){m->prepared|=UINT64_C(1)<<c;m->owner_busy[c]=false;}
 tam_unlock(m);return ok?true:tam_fail(m);
}
bool tam_finish(struct tr_atomic_maintenance *m,struct tam_ticket t) {
 if(!m||!m->owners[0])return false;
 tam_lock(m);struct typed_directory *d=m->owners[0]->bridge->directory.directory;
 bool ok=tam_valid_locked(m,t)&&m->state==TAM_PREPARING&&m->prepared==3&&m->reset==3&&m->revoked==3&&
  !m->owner_busy[0]&&!m->owner_busy[1]&&!d->operations&&!d->ops;
 for(unsigned c=0;c<TAM_OWNERS&&ok;c++) {
  struct tr_atomic_owner *o=m->owners[c];ok=tam_stamp_eq(o->config.stamp,m->next_stamp)&&tam_quiet_locked(o);
  for(size_t n=0;n<m->root_count[c]&&ok;n++) {
   struct tam_root *r=&m->roots[c][n];unsigned pins=r->item==o->registry->active?1:0;
   ok=tam_root_locked(m,c,r,true)&&r->item->pins==pins&&r->binding->slot->directory.record.pins==pins&&!r->binding->slot->directory.record.refills;
  }
 }
 if(ok)m->state=TAM_REOPENING;
 tam_unlock(m);if(!ok)return false;
 struct ad_entry_stamp next;
 if(!tam_stopped(m,t)||!tam_source_clean(m)||!ad_updater_gate_reopen(m->owners[0]->config.gate,t.stamp,true,&next)||
    !tam_stamp_eq(next,m->next_stamp)||!m->hooks.open_entry(m->context,t))return tam_fail(m);
 tam_lock(m);ok=tam_valid_locked(m,t)&&m->state==TAM_REOPENING;
 if(ok)m->state=TAM_OPEN;
 tam_unlock(m);return ok?true:tam_fail(m);
}
