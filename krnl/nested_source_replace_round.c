#include "nested_source_replace_round.h"
#include "nested_owner_sparse.h"
static uint32_t take(const uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void give(uint32_t *p,uint32_t n){__atomic_store_n(p,n,__ATOMIC_RELEASE);}
static bool current(struct nested_source_replace_round *r){
 return r&&(!r->active_generation||(__atomic_load_n(r->active_generation,__ATOMIC_ACQUIRE)==r->generation&&
  r->chain_fault&&!__atomic_load_n(r->chain_fault,__ATOMIC_ACQUIRE)));
}
static bool health(struct nested_source_replace_round *r){return current(r)&&(!r->healthy||r->healthy(r->context));}
static bool fail(struct nested_source_replace_round *r){
 if(!r)return false;
 if(r->active_generation){
  r->metadata_lock(r->metadata_context);
  if(__atomic_load_n(r->active_generation,__ATOMIC_ACQUIRE)!=r->generation){
   r->metadata_unlock(r->metadata_context);return false;
  }
  /* This closes handoff BEFORE unlocked abort/platform fault propagation. */
  __atomic_store_n(r->chain_fault,1,__ATOMIC_RELEASE);
  r->metadata_unlock(r->metadata_context);
 }
 if(take(&r->bootstrapped)){
  /* A peer timeout before first command can overlap tsr_close writing its
   * output ticket. Snapshot the coordinator's protected ticket instead. */
  r->metadata_lock(r->metadata_context);struct tsr_ticket t=r->replacement.ticket;
  r->metadata_unlock(r->metadata_context);
  if(t.nonce)tsr_abort(&r->replacement,t);
 }
 if(r->fault)r->fault(r->context);
 nested_domain_dispatch_markfail(r->dispatch);return false;
}
static bool wait(struct nested_source_replace_round *r,uint32_t *p,uint32_t n){
 for(unsigned i=0;i<NSRR_POLLS;i++){
  if(!health(r))return false;
  uint32_t got=take(p);if(got==n)return true;if(got>n)return false;
  __asm__ volatile("pause":::"memory");
 }return false;
}
static bool same(struct nested_domain_maintenance_ticket a,struct nested_domain_maintenance_ticket b){
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.epoch==b.epoch&&a.purpose==b.purpose;
}
static bool plan(void *p,struct tsr_ticket t,struct nested_domain_maintenance_ticket *out){
 struct nested_source_replace_round *r=p;
 return t.nonce&&health(r)&&take(&r->arrived[0])&&take(&r->arrived[1])&&
  take(&r->parked[0])&&take(&r->parked[1])&&nested_domain_dispatch_source_plan(r->dispatch,out);
}
static bool close_entry(void *p,struct tsr_ticket t){
 struct nested_source_replace_round *r=p;struct nested_domain_maintenance_ticket actual;
 /* The coordinator already bound the full expected ticket before this hook. */
 return health(r)&&nested_domain_dispatch_maintenance_begin(r->dispatch,NESTED_DOMAIN_SOURCE_REPLACE,&actual)&&same(t.domain,actual);
}
static bool stopped(void *p,unsigned c,struct tsr_ticket t){
 struct nested_source_replace_round *r=p;
 return c<2&&health(r)&&take(&r->arrived[c])&&take(&r->parked[c])&&
  nested_domain_dispatch_source_stopped(r->dispatch,c,r->owners[c]->token,t.domain);
}
static bool park(void *p,unsigned c,struct tsr_ticket t,const struct tsr_root *roots,size_t count,struct tsr_receipt *out){
 struct nested_source_replace_round *r=p;
 return c<2&&nosr_bind(&r->refs[c],t)&&nosr_park(&r->refs[c],t,roots,count,out);
}
static bool restore(void *p,unsigned c,struct tsr_ticket t,const struct tsr_root *root,struct tsr_receipt *out){
 struct nested_source_replace_round *r=p;return c<2&&nosr_restore(&r->refs[c],t,root,out);
}
static bool local(struct nested_source_replace_round *r,unsigned c,unsigned command){
 bool ok=false;
 if(c>1||!take(&r->parked[c])||take(&r->ack[c])+1!=command)return false;
 switch(command){
 case NSRR_RETIRE:ok=tsr_retire_owner(&r->replacement,c,r->ticket);break;
 case NSRR_PREPARE:
  ok=tsr_prepare_owner(&r->replacement,c,r->ticket);
  if(ok){ /* SELECT bypassed the ordinary sparse wrapper, but creates a root. */
   struct nested_owner_sparse *s=r->owners[c]->sparse;
   if(s->creations==UINT64_MAX)ok=false;else s->creations++;
  }break;
 case NSRR_VALIDATE:ok=nosr_validate(&r->refs[c],r->ticket,&r->validated[c]);break;
 default:return false;
 }
 if(ok)give(&r->ack[c],command);
 return ok;
}
static bool both(struct nested_source_replace_round *r,unsigned command){
 give(&r->command,command);return local(r,0,command)&&wait(r,&r->ack[1],command);
}
static bool complete(void *p,struct tsr_ticket t){
 struct nested_source_replace_round *r=p;
 if(!health(r)||!same(t.domain,r->ticket.domain)||!both(r,NSRR_VALIDATE))return false;
 /* Acquired ACKs attest owner-local actual VMREAD and unchanged launch. AP
  * cannot modify VMCS references again until the exact final release. */
 for(unsigned c=0;c<2;c++)if(!r->validated[c].success||r->validated[c].owner!=c||
   !nosr_ticket_equal(r->validated[c].ticket,t)||
   !nosr_root_equal(r->validated[c].root,r->replacement.fresh[c])||
   r->refs[c].phase!=NOSR_VALIDATED||
   !nested_domain_dispatch_maintenance_ack(r->dispatch,c,t.domain))return false;
 return nested_domain_dispatch_maintenance_publish(r->dispatch,t.domain)&&
  nested_domain_dispatch_maintenance_complete(r->dispatch,t.domain);
}
bool nsrr_empty_ready(struct nested_owner_atomic *a,void *p){
 struct nested_source_replace_round *r=p;
 if(!r||!a||!a->sparse||!r->dispatch||!r->change||!a->atomic.initialized||a->sparse->pool.owner>1)return false;
 unsigned c=a->sparse->pool.owner;
 if(r->owners[c]!=a||a->config.current_cpu(a->config.context)!=c||take(&r->empty[c])||
  a->sparse->contexts.count||a->sparse->contexts.items||a->sparse->creations||a->atomic.maintenance||a->atomic.replacement)return false;
 give(&r->empty[c],1);
 if(c)return wait(r,&r->bootstrapped,1)?true:fail(r);
 if(!wait(r,&r->empty[1],1))return fail(r);
 r->metadata_lock=a->atomic.config.lock;r->metadata_unlock=a->atomic.config.unlock;r->metadata_context=a->atomic.config.context;
 const struct tsr_hooks hooks={plan,close_entry,stopped,park,restore,complete};
 if(!tsr_init(&r->replacement,&r->owners[0]->atomic,&r->owners[1]->atomic,
  &r->owners[0]->provider,&r->owners[1]->provider,&r->owners[0]->config,&r->owners[1]->config,&hooks,r))return fail(r);
 give(&r->bootstrapped,1);return true;
}
bool nsrr_before_entry(struct nested_source_replace_round *r,unsigned c){
 if(!r||c>1||!health(r))return false;
 /* No owner-local reads while the coordinator rearms/rebinds. */
 if(!take(&r->parked[c]))return true;
 return take(&r->committed[c])&&tsr_released(&r->replacement,r->ticket);
}
bool nsrr_run(struct nested_source_replace_round *r,unsigned c){
 if(!r||c>1||!take(&r->bootstrapped)||take(&r->arrived[c])||!health(r))return false;
 struct nested_owner_atomic *a=r->owners[c];
 if(!nested_owner_atomic_stopped(a)||!a->sparse->hardware_item||!a->atomic.replacement)return fail(r);
 give(&r->parked[c],1);give(&r->arrived[c],1);
 if(c){
  for(unsigned command=NSRR_RETIRE;command<=NSRR_VALIDATE;command++)
   if(!wait(r,&r->command,command)||!local(r,c,command))return fail(r);
  bool released=false;
  for(unsigned i=0;i<NSRR_POLLS&&health(r);i++){
   if(tsr_released(&r->replacement,r->ticket)){released=true;break;}
   __asm__ volatile("pause":::"memory");
  }
  if(!released){
   /* A stale timeout cannot revoke release: abort and finish serialize in
    * tsr. Re-query after abort to tell which won, with no second commit flag. */
   tsr_abort(&r->replacement,r->ticket);
   if(!tsr_released(&r->replacement,r->ticket))return fail(r);
  }
 }else{
  if(!wait(r,&r->arrived[1],1)||!tsr_close(&r->replacement,&r->ticket))return fail(r);
  enum tsr_drain d;
  do{d=tsr_drain(&r->replacement,r->ticket);}while(d==TSR_DRAIN_WAIT&&health(r));
  if(d!=TSR_DRAIN_READY||!both(r,NSRR_RETIRE)||
   !tsr_replace(&r->replacement,r->ticket,r->change,r->change_context)||
   !both(r,NSRR_PREPARE)||!tsr_finish(&r->replacement,r->ticket))return fail(r);
 }
 if(!nosr_commit(&r->refs[c],r->ticket))return fail(r);
 give(&r->committed[c],1);return true;
}

bool nsrr_retain_stop(struct nested_source_replace_round *r){return fail(r);}
bool nsrr_init_successor_locked(struct nested_source_replace_round *r,struct tsr_successor_handoff *h){
 if(!r||!h||&r->replacement!=h->next||r->bootstrapped||!r->change||!r->dispatch||
    r->active_generation!=h->active_generation||r->generation!=h->next_generation||
    !r->chain_fault||__atomic_load_n(r->chain_fault,__ATOMIC_ACQUIRE))return false;
 for(unsigned c=0;c<2;c++)if(!r->owners[c]||&r->owners[c]->atomic!=h->prior->owners[c]||
   r->refs[c].phase!=NOSR_READY||r->refs[c].failure!=NOSR_OK||r->refs[c].cpu!=c||
   r->refs[c].config.replacement!=h->next)return false;
 const struct tsr_hooks hooks={plan,close_entry,stopped,park,restore,complete};
 /* Initialize the last local publication field before active-generation
  * release. An AP may observe that release before this function returns. */
 give(&r->bootstrapped,1);
 if(!tsr_init_successor_locked(h,&hooks,r)){give(&r->bootstrapped,0);return false;}
 return true;
}
