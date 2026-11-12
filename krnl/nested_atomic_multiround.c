#include "nested_atomic_multiround.h"
#include "nested_owner_sparse.h"
static uint64_t acquire(const uint64_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void release(uint64_t *p,uint64_t n){__atomic_store_n(p,n,__ATOMIC_RELEASE);}
static uint32_t acquire32(const uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void release32(uint32_t *p,uint32_t n){__atomic_store_n(p,n,__ATOMIC_RELEASE);}
static bool healthy(struct nested_atomic_multiround *m){
 struct nested_atomic_retrack *r=m->runtime;return !nart_failed(r)&&(!r->poll||r->poll(r->context));
}
static void lock(struct nested_atomic_multiround *m){
 struct tr_atomic_config *c=&m->runtime->refs[0].owner->atomic.config;c->lock(c->context);
}
static void unlock(struct nested_atomic_multiround *m){
 struct tr_atomic_config *c=&m->runtime->refs[0].owner->atomic.config;c->unlock(c->context);
}
static bool fail(struct nested_atomic_multiround *m,uint64_t g){
 struct nested_atomic_retrack *r=m->runtime;
 bool initialized=acquire32(&r->bootstrapped)!=0;
 if(initialized)lock(m);
 /* A stale observer cannot poison a later round. Genuine current-round
  * postcommit failures still retain-stop, and must not erase the commit. */
 bool current=!g||acquire(&m->active)==g;
 if(current)release32(&r->fault,1);
 if(initialized)unlock(m);
 if(current){if(initialized)tam_abort(&r->maintenance);nested_domain_dispatch_markfail(r->dispatch);}
 return false;
}
bool namr_release_timeout(struct nested_atomic_multiround *m,uint64_t g){
 if(!m||!m->runtime||!g||g>NAMR_MAX_GENERATION||!acquire32(&m->runtime->bootstrapped))return false;
 lock(m);uint64_t active=acquire(&m->active),released=acquire(&m->released);
 bool committed=released>=g;
 bool poison=active==g&&!committed;
 if(poison)release32(&m->runtime->fault,1);
 unlock(m);
 if(poison){tam_abort(&m->runtime->maintenance);nested_domain_dispatch_markfail(m->runtime->dispatch);}
 return committed;
}
static bool wait_value(struct nested_atomic_multiround *m,uint64_t *p,uint64_t value){
 for(unsigned n=0;n<NART_POLLS;n++){
  if(!healthy(m))return false;
  uint64_t got=acquire(p);if(got==value)return true;if(got>value)return false;
  __asm__ volatile("pause":::"memory");
 }
 return false;
}
static bool wait_boot(struct nested_atomic_multiround *m,uint32_t *p){
 for(unsigned n=0;n<NART_POLLS;n++){
  if(!healthy(m))return false;
  if(acquire32(p)==1)return true;
  __asm__ volatile("pause":::"memory");
 }
 return false;
}
static uint64_t tag(uint64_t g,enum nart_command command){return (g-1)*NAMR_TAG_STRIDE+(unsigned)command;}
static bool domain_equal(struct nested_domain_maintenance_ticket a,struct nested_domain_maintenance_ticket b){
 return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&a.epoch==b.epoch&&a.purpose==b.purpose;
}
static bool stopped(void *p,unsigned c,struct tam_ticket t){
 struct nested_atomic_multiround *m=p;struct nested_atomic_retrack *r=m->runtime;uint64_t g=acquire(&m->active);
 if(c>=2||!g||!healthy(m)||acquire(&m->bound[c])!=g||!noam_ticket_equal(t,r->ticket))return false;
 const struct nart_evidence *e=&r->evidence[c];
 return e->cpu==c&&noam_ticket_equal(e->ticket,t)&&domain_equal(e->domain_ticket,r->domain_ticket)&&
  nested_domain_dispatch_maintenance_stopped(r->dispatch,c,e->token,e->domain_ticket);
}
static bool remove_refs(void *p,unsigned c,struct tam_ticket t){
 struct nested_atomic_multiround *m=p;return c<2&&stopped(p,c,t)&&noam_remove(&m->runtime->refs[c],t);
}
static bool flush(void *p,unsigned c,struct tam_ticket t,const struct tam_root *root,struct tam_flush_receipt *receipt){
 struct nested_atomic_multiround *m=p;return c<2&&stopped(p,c,t)&&noam_flush(&m->runtime->refs[c],t,root,receipt);
}
static bool restore_refs(void *p,unsigned c,struct tam_ticket t,const struct tam_root *root){
 struct nested_atomic_multiround *m=p;return c<2&&stopped(p,c,t)&&noam_restore(&m->runtime->refs[c],t,root);
}
static bool local_command(struct nested_atomic_multiround *m,unsigned c,uint64_t g,enum nart_command command){
 struct nested_atomic_retrack *r=m->runtime;struct nested_owner_atomic_maintenance *ref=&r->refs[c];bool ok=false;
 if(acquire(&m->active)!=g||acquire(&m->command)!=tag(g,command)||acquire(&m->arrived[c])!=g||
    acquire(&m->ack[c])>=tag(g,command))return false;
 switch(command){
 case NART_BIND:
  ok=noam_bind(ref,r->ticket,r->domain_ticket);
  if(ok){r->evidence[c]=(struct nart_evidence){r->ticket,r->domain_ticket,ref->owner->token,c};release(&m->bound[c],g);}
  break;
 case NART_REVOKE:ok=tam_revoke(&r->maintenance,c,r->ticket);break;
 case NART_RESET:ok=tam_reset(&r->maintenance,c,r->ticket);break;
 case NART_PREPARE:ok=tam_prepare(&r->maintenance,c,r->ticket)&&nested_domain_dispatch_maintenance_ack(r->dispatch,c,r->domain_ticket);break;
 default:return false;
 }
 if(ok)release(&m->ack[c],tag(g,command));
 return ok;
}
static bool command_both(struct nested_atomic_multiround *m,uint64_t g,enum nart_command command){
 release(&m->command,tag(g,command));
 return local_command(m,0,g,command)&&wait_value(m,&m->ack[1],tag(g,command));
}
static bool close_entry(void *p,struct tam_ticket t){
 struct nested_atomic_multiround *m=p;struct nested_atomic_retrack *r=m->runtime;uint64_t g=acquire(&m->active);
 if(!g||acquire(&m->arrived[0])!=g||acquire(&m->arrived[1])!=g||
    acquire(&m->completed[0])!=g-1||acquire(&m->completed[1])!=g-1||!noam_ticket_equal(t,r->ticket)||
    !nested_domain_dispatch_maintenance_begin(r->dispatch,NESTED_DOMAIN_AD_RESET,&r->domain_ticket))return false;
 return command_both(m,g,NART_BIND);
}
static bool open_entry(void *p,struct tam_ticket t){
 struct nested_atomic_multiround *m=p;struct nested_atomic_retrack *r=m->runtime;uint64_t g=acquire(&m->active);
 if(!stopped(p,0,t)||!stopped(p,1,t)||acquire(&m->ack[0])!=tag(g,NART_PREPARE)||
    acquire(&m->ack[1])!=tag(g,NART_PREPARE)||r->refs[0].phase!=NOAM_RESTORED||
    r->refs[1].phase!=NOAM_RESTORED||acquire(&m->released)!=g-1)return false;
 return nested_domain_dispatch_maintenance_publish(r->dispatch,r->domain_ticket)&&
  nested_domain_dispatch_maintenance_complete(r->dispatch,r->domain_ticket);
}
bool namr_empty_ready(struct nested_owner_atomic *a,void *p){
 struct nested_atomic_multiround *m=p;struct nested_atomic_retrack *r=m?m->runtime:0;
 if(!r||!a||!r->dispatch||!r->outer_stamp||!a->atomic.initialized||!a->sparse||a->sparse->pool.owner>1)return false;
 unsigned c=a->sparse->pool.owner;
 if(r->refs[c].owner!=a||a->config.current_cpu(a->config.context)!=c||acquire32(&r->empty[c])||
    a->sparse->contexts.count||a->sparse->contexts.items||a->sparse->creations||a->atomic.maintenance)return false;
 r->refs[c].maintenance=&r->maintenance;release32(&r->empty[c],1);
 if(c)return wait_boot(m,&r->bootstrapped)?true:fail(m,0);
 if(!wait_boot(m,&r->empty[1]))return fail(m,0);
 const struct tam_hooks hooks={close_entry,stopped,remove_refs,flush,restore_refs,open_entry,0,0};
 if(!tam_init(&r->maintenance,&r->refs[0].owner->atomic,&r->refs[1].owner->atomic,&hooks,m))return fail(m,0);
 release32(&r->bootstrapped,1);return true;
}
static bool complete(struct nested_atomic_multiround *m,unsigned c,uint64_t g){
 struct nested_atomic_retrack *r=m->runtime;
 if(acquire(&m->released)!=g||!healthy(m)||!noam_commit(&r->refs[c],r->ticket,*r->outer_stamp))return fail(m,g);
 /* Last shared-payload access. A faster peer cannot reuse TAM or the ticket
  * until this acquire/release handoff, even if its next arrival is earlier. */
 release(&m->completed[c],g);
 /* Neither owner may resume a guest while its peer is still validating the
  * committed physical reference. No payload access follows publication. */
 return wait_value(m,&m->completed[c^1],g)?true:fail(m,g);
}
bool namr_round(struct nested_atomic_multiround *m,unsigned c,uint64_t g){
 struct nested_atomic_retrack *r=m?m->runtime:0;
 if(!r||c>1||!g||g>NAMR_MAX_GENERATION||!acquire32(&r->bootstrapped)||nart_failed(r))return false;
 struct nested_owner_atomic *a=r->refs[c].owner;
 if(!a||!a->config.current_cpu||a->config.current_cpu(a->config.context)!=c)return false;
 lock(m);
 bool admitted=acquire(&m->claimed[c])==g-1&&acquire(&m->completed[c])==g-1&&acquire(&m->completed[c^1])==g-1&&
  acquire(&m->released)==g-1&&(acquire(&m->active)==g-1||acquire(&m->active)==g)&&!nart_failed(r)&&r->maintenance.state==TAM_OPEN&&
  !r->maintenance.operations[0].live&&!r->maintenance.operations[1].live&&
  !r->maintenance.operation_nonce[0]&&!r->maintenance.operation_nonce[1]&&
  !r->maintenance.owner_busy[0]&&!r->maintenance.owner_busy[1];
 if(admitted){release(&m->active,g);release(&m->claimed[c],g);}
 unlock(m);if(!admitted)return false;
 /* The admission above rejects an early next call without mutation. Prior
  * completion precedes every reusable payload access, not merely tam_close. */
 if(!nested_owner_atomic_stopped(a)||!a->atomic.maintenance||!a->sparse->hardware_item)return fail(m,g);
 r->arrival_stamp[c]=a->atomic.config.stamp;release(&m->arrived[c],g);
 if(c){
  for(unsigned next=NART_BIND;next<=NART_PREPARE;next++)
   if(!wait_value(m,&m->command,tag(g,next))||!local_command(m,c,g,next))return fail(m,g);
  if(!wait_value(m,&m->released,g)&&!namr_release_timeout(m,g))return false;
  return complete(m,c,g);
 }
 if(!wait_value(m,&m->arrived[1],g))return fail(m,g);
 struct tam_ticket t0={r->arrival_stamp[0],1},t1={r->arrival_stamp[1],1},outer={*r->outer_stamp,1};
 if(!noam_ticket_equal(t0,t1)||!noam_ticket_equal(t0,outer)||!tam_close(&r->maintenance,&r->ticket))return fail(m,g);
 enum tam_drain_result drained;
 do {drained=tam_drain(&r->maintenance,r->ticket);}while(drained==TAM_DRAIN_WAIT&&healthy(m));
 if(drained!=TAM_DRAIN_READY||!command_both(m,g,NART_REVOKE)||!tam_clear(&r->maintenance,r->ticket)||
    !command_both(m,g,NART_RESET)||!command_both(m,g,NART_PREPARE)||!tam_finish(&r->maintenance,r->ticket))return fail(m,g);
 lock(m);bool commit=healthy(m)&&r->maintenance.state==TAM_OPEN&&acquire(&m->active)==g&&acquire(&m->released)==g-1;
 if(commit){*r->outer_stamp=r->maintenance.next_stamp;release(&m->command,tag(g,NART_RELEASE));release(&m->released,g);}
 unlock(m);return commit?complete(m,c,g):fail(m,g);
}
