#include "nested_atomic_retrack.h"
#include "nested_owner_sparse.h"
static uint32_t acquire(const uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void release(uint32_t *p,uint32_t n){__atomic_store_n(p,n,__ATOMIC_RELEASE);}
bool nart_failed(const struct nested_atomic_retrack *r){return !r||acquire(&r->fault)!=0;}
static bool healthy(struct nested_atomic_retrack *r){return !nart_failed(r)&&(!r->poll||r->poll(r->context));}
static void metadata_lock(struct nested_atomic_retrack *r){
 struct tr_atomic_config *c=&r->refs[0].owner->atomic.config;c->lock(c->context);
}
static void metadata_unlock(struct nested_atomic_retrack *r){
 struct tr_atomic_config *c=&r->refs[0].owner->atomic.config;c->unlock(c->context);
}
static bool abort_round(struct nested_atomic_retrack *r){
 bool initialized=acquire(&r->bootstrapped)!=0;
 /* Final epoch/release commit and failed-round publication share the SAME
  * metadata serialization. An AP timeout after tam_finish cannot race past
  * the final check and publish a successful release from a poisoned round. */
 if(initialized)metadata_lock(r);
 release(&r->fault,1);
 if(initialized)metadata_unlock(r);
 if(initialized)tam_abort(&r->maintenance);
 nested_domain_dispatch_markfail(r->dispatch);return false;
}
static bool wait_value(struct nested_atomic_retrack *r,uint32_t *p,uint32_t value){
 for(unsigned n=0;n<NART_POLLS;n++){
  if(!healthy(r))return false;
  uint32_t got=acquire(p);
  if(got==value)return true;
  if(got>value)return false;
  __asm__ volatile("pause":::"memory");
 }
 return false;
}
static bool domain_equal(struct nested_domain_maintenance_ticket a,struct nested_domain_maintenance_ticket b){
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.epoch==b.epoch&&a.purpose==b.purpose;
}
static bool stopped(void *opaque,unsigned c,struct tam_ticket t){
 struct nested_atomic_retrack *r=opaque;
 if(c>=2||!healthy(r)||!acquire(&r->bound[c])||!noam_ticket_equal(t,r->ticket))return false;
 const struct nart_evidence *e=&r->evidence[c];
 /* Remote BSP evidence, not owner-local noam_stopped (which checks CPU). */
 return e->cpu==c&&noam_ticket_equal(e->ticket,t)&&domain_equal(e->domain_ticket,r->domain_ticket)&&
  nested_domain_dispatch_maintenance_stopped(r->dispatch,c,e->token,e->domain_ticket);
}
static bool remove_refs(void *p,unsigned c,struct tam_ticket t){
 struct nested_atomic_retrack *r=p;return c<2&&stopped(r,c,t)&&noam_remove(&r->refs[c],t);
}
static bool flush(void *p,unsigned c,struct tam_ticket t,const struct tam_root *root,struct tam_flush_receipt *receipt){
 struct nested_atomic_retrack *r=p;return c<2&&stopped(r,c,t)&&noam_flush(&r->refs[c],t,root,receipt);
}
static bool restore_refs(void *p,unsigned c,struct tam_ticket t,const struct tam_root *root){
 struct nested_atomic_retrack *r=p;return c<2&&stopped(r,c,t)&&noam_restore(&r->refs[c],t,root);
}
static bool local_command(struct nested_atomic_retrack *r,unsigned c,enum nart_command command){
 struct nested_owner_atomic_maintenance *ref=&r->refs[c];bool ok=false;
 switch(command){
 case NART_BIND:
  ok=noam_bind(ref,r->ticket,r->domain_ticket);
  if(ok){r->evidence[c]=(struct nart_evidence){r->ticket,r->domain_ticket,ref->owner->token,c};release(&r->bound[c],1);}
  break;
 case NART_REVOKE:ok=tam_revoke(&r->maintenance,c,r->ticket);break;
 case NART_RESET:ok=tam_reset(&r->maintenance,c,r->ticket);break;
 case NART_PREPARE:
  ok=tam_prepare(&r->maintenance,c,r->ticket)&&
   nested_domain_dispatch_maintenance_ack(r->dispatch,c,r->domain_ticket);
  break;
 default:return false;
 }
 if(ok)release(&r->ack[c],command);
 return ok;
}
static bool command_both(struct nested_atomic_retrack *r,enum nart_command command){
 release(&r->command,command);
 return local_command(r,0,command)&&wait_value(r,&r->ack[1],command);
}
static bool close_entry(void *opaque,struct tam_ticket t){
 struct nested_atomic_retrack *r=opaque;
 if(!acquire(&r->arrived[0])||!acquire(&r->arrived[1])||!noam_ticket_equal(t,r->ticket)||
    !nested_domain_dispatch_maintenance_begin(r->dispatch,NESTED_DOMAIN_AD_RESET,&r->domain_ticket))return false;
 return command_both(r,NART_BIND);
}
static bool open_entry(void *opaque,struct tam_ticket t){
 struct nested_atomic_retrack *r=opaque;
 if(!stopped(r,0,t)||!stopped(r,1,t)||acquire(&r->ack[0])!=NART_PREPARE||acquire(&r->ack[1])!=NART_PREPARE||
    r->refs[0].phase!=NOAM_RESTORED||r->refs[1].phase!=NOAM_RESTORED||acquire(&r->released))return false;
 /* Physical metadata opens here. Owners remain parked, TAM software admission
  * is still closed, and the outer stamp is committed ONLY after tam_finish. */
 return nested_domain_dispatch_maintenance_publish(r->dispatch,r->domain_ticket)&&
  nested_domain_dispatch_maintenance_complete(r->dispatch,r->domain_ticket);
}
bool nart_empty_ready(struct nested_owner_atomic *a,void *opaque){
 struct nested_atomic_retrack *r=opaque;
 if(!r||!a||!r->dispatch||!r->outer_stamp||!a->atomic.initialized||!a->sparse||a->sparse->pool.owner>1)return false;
 unsigned c=a->sparse->pool.owner;
 if(r->refs[c].owner!=a||a->config.current_cpu(a->config.context)!=c||acquire(&r->empty[c])||
    a->sparse->contexts.count||a->sparse->contexts.items||a->sparse->creations||a->atomic.maintenance)return false;
 r->refs[c].maintenance=&r->maintenance;
 release(&r->empty[c],1);
 if(c)return wait_value(r,&r->bootstrapped,1)?true:abort_round(r);
 if(!wait_value(r,&r->empty[1],1))return abort_round(r);
 const struct tam_hooks hooks={close_entry,stopped,remove_refs,flush,restore_refs,open_entry,0,0};
 if(!tam_init(&r->maintenance,&r->refs[0].owner->atomic,&r->refs[1].owner->atomic,&hooks,r))return abort_round(r);
 release(&r->bootstrapped,1);return true;
}
bool nart_round(struct nested_atomic_retrack *r,unsigned c){
 if(!r||c>1||!acquire(&r->bootstrapped)||acquire(&r->arrived[c])||nart_failed(r))return false;
 struct nested_owner_atomic *a=r->refs[c].owner;
 if(!nested_owner_atomic_stopped(a)||!a->atomic.maintenance||!a->sparse->hardware_item)return abort_round(r);
 r->arrival_stamp[c]=a->atomic.config.stamp;release(&r->arrived[c],1);
 if(c){
  for(unsigned next=NART_BIND;next<=NART_PREPARE;next++)
   if(!wait_value(r,&r->command,next)||!local_command(r,c,next))return abort_round(r);
  if(!wait_value(r,&r->released,1)){
   /* A timeout observed before its last load is not a failed round if the
    * serialized commit already won. Otherwise latch failure before BSP can
    * commit. Never publish an abort after a stale timeout alone. */
   metadata_lock(r);bool committed=acquire(&r->released)==1;
   if(!committed)release(&r->fault,1);
   metadata_unlock(r);
   if(!committed)return abort_round(r);
  }
  struct tam_ticket published={*r->outer_stamp,r->ticket.nonce};
  struct tam_ticket expected={r->maintenance.next_stamp,r->ticket.nonce};
  return noam_ticket_equal(published,expected)&&healthy(r)?true:abort_round(r);
 }
 /* Must precede tam_close: TAM_STOPPING would otherwise deny a late AP's
  * legitimate fault/entry on its path to this checkpoint. */
 if(!wait_value(r,&r->arrived[1],1))return abort_round(r);
 struct tam_ticket t0={r->arrival_stamp[0],1},t1={r->arrival_stamp[1],1},outer={*r->outer_stamp,1};
 if(!noam_ticket_equal(t0,t1)||!noam_ticket_equal(t0,outer))return abort_round(r);
 if(!tam_close(&r->maintenance,&r->ticket))return abort_round(r);
 enum tam_drain_result drained;
 do {drained=tam_drain(&r->maintenance,r->ticket);}while(drained==TAM_DRAIN_WAIT&&healthy(r));
 if(drained!=TAM_DRAIN_READY||!command_both(r,NART_REVOKE)||!tam_clear(&r->maintenance,r->ticket)||
    !command_both(r,NART_RESET)||!command_both(r,NART_PREPARE)||!tam_finish(&r->maintenance,r->ticket))return abort_round(r);
 metadata_lock(r);
 bool commit=healthy(r)&&r->maintenance.state==TAM_OPEN;
 if(commit){
  *r->outer_stamp=r->maintenance.next_stamp;
  release(&r->command,NART_RELEASE);release(&r->released,1);
 }
 metadata_unlock(r);
 return commit?true:abort_round(r);
}
