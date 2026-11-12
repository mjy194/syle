/* Independent N43 review: production core; only privileged primitives mocked.
 * Preserved candidate host fixture is supplemented, not changed. */
#include "n43_tests_base.inc"
static uint32_t peer_held,peer_resume,bsp_waiting;
static bool peer_fail;
static bool (*saved_open)(void *,struct tam_ticket);
static unsigned replay_checks;
static struct multifixture *review_ready(void){
 struct multifixture *x=create_multi();current_test=x;
 for(struct nested_ept_context_item *it=x->g->f[0].sparse.contexts.items;it;it=it->next)
  if(it->key==0x1005e)select_item(&x->g->f[0],it);
 assert(x->g->f[0].sparse.hardware_item->key==0x1005e);
 assert(x->g->f[1].sparse.hardware_item->key==0x1805e);
 execute_round(x,true);snapshot_first(x);all_roots(x,1);x->generation=2;return x;
}
static void dispose_review(struct multifixture *x){free(x->g);free(x);current_test=0;}
static bool reject_poll(void *p){(void)p;return false;}
static void retained_guards(void){
 for(unsigned k=0;k<10;k++){
  struct multifixture *x=review_ready();struct nested_atomic_retrack *r=&x->g->runtime;
  if(k==0)r->maintenance.owner_busy[0]=true;
  if(k==1)r->maintenance.owner_busy[1]=true;
  if(k==2)r->maintenance.operations[0].live=true;
  if(k==3)r->maintenance.operations[1].live=true;
  if(k==4)r->maintenance.operation_nonce[0]=123;
  if(k==5)r->maintenance.operation_nonce[1]=123;
  if(k==6)__atomic_store_n(&x->multi.completed[0],0,__ATOMIC_RELEASE);
  if(k==7)__atomic_store_n(&x->multi.completed[1],0,__ATOMIC_RELEASE);
  if(k==8)__atomic_store_n(&x->multi.released,0,__ATOMIC_RELEASE);
  uint64_t requested=2;
  if(k==9){
   x->multi.active=x->multi.released=NAMR_MAX_GENERATION;
   for(unsigned c=0;c<2;c++)x->multi.claimed[c]=x->multi.completed[c]=NAMR_MAX_GENERATION;
   requested=NAMR_MAX_GENERATION+1;
  }
  struct nested_atomic_multiround before=x->multi;struct tam_ticket ticket=r->ticket;
  r->poll=reject_poll;actual_cpu=0;
  assert(!namr_round(&x->multi,0,requested));
  assert(!memcmp(&before,&x->multi,sizeof before)&&!nart_failed(r));
  assert(noam_ticket_equal(ticket,r->ticket)&&r->maintenance.state==TAM_OPEN&&x->g->outer.epoch==2);
  dispose_review(x);
 }
 puts("PASS independent retained operations/nonces/busy, consumed/release and isolated overflow guards reject before mutation");
}
static void phase_guards(void){
 for(unsigned phase=NOAM_FRESH;phase<=NOAM_UNCERTAIN;phase++){
  if(phase==NOAM_COMMITTED)continue;
  struct multifixture *x=review_ready();struct nested_atomic_retrack *r=&x->g->runtime;
  r->refs[0].phase=(enum noam_phase)phase;execute_round(x,false);
  assert(nart_failed(r)&&get(&x->multi.released)==1&&x->g->outer.epoch==2);
  assert(r->refs[0].ticket.nonce==1&&r->refs[0].committed_stamp.epoch==2);
  assert(m.catalog.references==4&&x->g->f[0].sparse.hardware_item->pins==1);
  dispose_review(x);
 }
 puts("PASS independent all noncommitted adapter states, including forged FRESH, refuse rearm");
}
static bool replay_open(void *p,struct tam_ticket t){
 struct nested_atomic_multiround *multi=p;struct nested_atomic_retrack *r=multi->runtime;
 struct multifixture *x=current_test;assert(x->generation==2&&actual_cpu==0);
 struct tr_atomic_maintenance before=r->maintenance;struct tam_ticket old=x->old_ticket;
 assert(tam_drain(&r->maintenance,old)==TAM_DRAIN_FAILED);
 assert(!tam_revoke(&r->maintenance,0,old)&&!tam_clear(&r->maintenance,old));
 assert(!tam_reset(&r->maintenance,0,old)&&!tam_prepare(&r->maintenance,0,old)&&!tam_finish(&r->maintenance,old));
 assert(!memcmp(&before,&r->maintenance,sizeof before));
 struct nested_owner_atomic_maintenance ref=r->refs[0];
 struct tam_flush_receipt receipt={0};
 assert(!noam_bind(&r->refs[0],old,x->old_domain)&&!noam_stopped(&r->refs[0],old));
 assert(!noam_remove(&r->refs[0],old)&&!noam_restore(&r->refs[0],old,&r->maintenance.roots[0][0]));
 assert(!noam_flush(&r->refs[0],old,&r->maintenance.roots[0][0],&receipt));
 assert(!noam_commit(&r->refs[0],old,r->maintenance.next_stamp));
 assert(!memcmp(&ref,&r->refs[0],sizeof ref));
 for(unsigned field=0;field<6;field++){
  struct tam_ticket bad=t;
  if(field==0)bad.nonce++;
  if(field==1)bad.stamp.domain_id++;
  if(field==2)bad.stamp.incarnation++;
  if(field==3)bad.stamp.context_id++;
  if(field==4)bad.stamp.epoch++;
  if(field==5)bad.stamp.lifetime++;
  assert(!noam_stopped(&r->refs[0],bad));
 }
 assert(!nested_domain_dispatch_maintenance_stopped(r->dispatch,0,r->refs[0].owner->token,x->old_domain));
 assert(!nested_domain_dispatch_maintenance_ack(r->dispatch,0,x->old_domain));
 assert(!nested_domain_dispatch_maintenance_publish(r->dispatch,x->old_domain));
 assert(!nested_domain_dispatch_maintenance_complete(r->dispatch,x->old_domain));
 replay_checks++;return saved_open(p,t);
}
static void replay_review(void){
 struct multifixture *x=review_ready();saved_open=x->g->runtime.maintenance.hooks.open_entry;
 x->g->runtime.maintenance.hooks.open_entry=replay_open;execute_round(x,true);
 assert(replay_checks==1);all_roots(x,2);dispose_review(x);
 puts("PASS independent round-one TAM/domain/NOAM replay across all ticketed APIs and all six current ticket fields");
}
static void timeout_review(void){
 struct multifixture *x=review_ready();pthread_t thread;void *result;current_test=x;
 __atomic_store_n(&x->returned[0],0,__ATOMIC_RELEASE);
 assert(!pthread_create(&thread,0,multi_thread,&x->g->f[0]));
 while(get(&x->multi.arrived[0])!=2&&!get32(&x->returned[0]))sched_yield();
 assert(get(&x->multi.active)==2&&get(&x->multi.released)==1);
 assert(namr_release_timeout(&x->multi,1)&&!nart_failed(&x->g->runtime));
 assert(!namr_release_timeout(&x->multi,3)&&!nart_failed(&x->g->runtime));
 assert(!namr_release_timeout(&x->multi,2));
 assert(!pthread_join(thread,&result)&&!result);
 assert(nart_failed(&x->g->runtime)&&x->g->outer.epoch==2&&get(&x->multi.released)==1);
 assert(x->g->runtime.maintenance.state==TAM_POISONED&&m.catalog.references==4);
 dispose_review(x);puts("PASS independent exact round-two timeout arbitration with old release still published");
}
static bool held_current(void *p,uint64_t *out){
 struct fixture *f=p;struct multifixture *x=current_test;
 if(actual_cpu==1&&x->generation==2&&get(&x->multi.released)==2&&x->g->runtime.refs[1].phase==NOAM_RESTORED){
  __atomic_store_n(&peer_held,1,__ATOMIC_RELEASE);
  while(!get32(&peer_resume))sched_yield();
  if(peer_fail)return false;
 }
 return hw_current(f,out);
}
static bool held_poll(void *p){
 struct multifixture *x=p;
 if(actual_cpu==0&&get(&x->multi.completed[0])==2&&get(&x->multi.completed[1])==1)
  __atomic_store_n(&bsp_waiting,1,__ATOMIC_RELEASE);
 return true;
}
static void completion_review(bool fail_peer){
 struct multifixture *x=review_ready();struct nested_atomic_retrack *r=&x->g->runtime;
 __atomic_store_n(&peer_held,0,__ATOMIC_RELAXED);__atomic_store_n(&peer_resume,0,__ATOMIC_RELAXED);
 __atomic_store_n(&bsp_waiting,0,__ATOMIC_RELAXED);peer_fail=fail_peer;
 r->refs[1].ops.current=held_current;r->poll=held_poll;r->context=x;
 pthread_t threads[2];void *results[2];
 for(unsigned c=0;c<2;c++){__atomic_store_n(&x->returned[c],0,__ATOMIC_RELAXED);assert(!pthread_create(&threads[c],0,multi_thread,&x->g->f[c]));}
 while(!get32(&peer_held))sched_yield();
 while(!get32(&bsp_waiting)&&!get32(&x->returned[0]))sched_yield();
 assert(get32(&bsp_waiting)&&!get32(&x->returned[0])&&!get32(&x->returned[1]));
 assert(x->g->outer.epoch==3&&get(&x->multi.released)==2&&get(&x->multi.completed[0])==2&&get(&x->multi.completed[1])==1);
 assert(r->refs[1].pending_item==x->g->f[1].sparse.hardware_item&&r->refs[1].pending_item->pins==1);
 __atomic_store_n(&peer_resume,1,__ATOMIC_RELEASE);
 for(unsigned c=0;c<2;c++)assert(!pthread_join(threads[c],&results[c]));
 if(fail_peer){
  assert(!results[0]&&!results[1]&&nart_failed(r)&&r->maintenance.state==TAM_POISONED);
  assert(x->g->outer.epoch==3&&get(&x->multi.released)==2);
  assert(r->refs[1].phase==NOAM_UNCERTAIN&&r->refs[1].pending_item&&r->refs[1].pending_item->pins==1);
 }else assert(results[0]&&results[1]&&!nart_failed(r)&&get(&x->multi.completed[1])==2);
 assert(m.catalog.references==4);dispose_review(x);
 printf("PASS independent delayed AP postcommit verification, peer failure=%u, no early BSP return\n",fail_peer);
}
static bool (*saved_flush)(void *,unsigned,struct tam_ticket,const struct tam_root *,struct tam_flush_receipt *);
static struct nested_ept_context_item *receipt_failed_item;
static unsigned receipt_failed_frees;
static uint64_t receipt_failed_generation;
static bool old_receipt_flush(void *p,unsigned c,struct tam_ticket t,const struct tam_root *root,struct tam_flush_receipt *out){
 bool ok=saved_flush(p,c,t,root,out);if(!ok||c)return ok;
 struct multifixture *x=current_test;
 for(unsigned n=0;n<2;n++)if(x->old_roots[c][n].item==root->item){
  *out=x->old_receipts[c][n];receipt_failed_item=root->item;
  receipt_failed_frees=x->g->f[c].frees;receipt_failed_generation=root->item->tree.generation;return true;
 }
 assert(0);return false;
}
static void receipt_review(void){
 struct multifixture *x=review_ready();struct nested_atomic_retrack *r=&x->g->runtime;
 receipt_failed_item=0;saved_flush=r->maintenance.hooks.flush;r->maintenance.hooks.flush=old_receipt_flush;
 execute_round(x,false);
 assert(receipt_failed_item&&receipt_failed_item->tree.generation==receipt_failed_generation);
 assert(x->g->f[0].frees==receipt_failed_frees&&nart_failed(r)&&m.catalog.references==4);
 assert(x->g->outer.epoch==2&&get(&x->multi.released)==1);
 dispose_review(x);puts("PASS independent exact old reset receipt replay rejects before affected lower-page release");
}
static bool (*saved_close)(void *,struct tam_ticket);
static unsigned evidence_kind;
static bool stale_bound_close(void *p,struct tam_ticket t){
 if(!saved_close(p,t))return false;
 struct nested_atomic_multiround *multi=p;struct multifixture *x=current_test;
 if(evidence_kind==0)__atomic_store_n(&multi->bound[0],1,__ATOMIC_RELEASE);
 else multi->runtime->evidence[0]=(struct nart_evidence){x->old_ticket,x->old_domain,x->g->f[0].owner.token,0};
 return true;
}
static bool stale_ack_open(void *p,struct tam_ticket t){
 struct nested_atomic_multiround *multi=p;
 __atomic_store_n(&multi->ack[1],NART_PREPARE,__ATOMIC_RELEASE);
 return saved_open(p,t);
}
static void evidence_review(void){
 for(unsigned kind=0;kind<3;kind++){
  struct multifixture *x=review_ready();struct nested_atomic_retrack *r=&x->g->runtime;
  unsigned frees=x->g->f[0].frees,writes=x->g->f[0].writes;evidence_kind=kind;
  if(kind<2){saved_close=r->maintenance.hooks.close_entry;r->maintenance.hooks.close_entry=stale_bound_close;}
  else{saved_open=r->maintenance.hooks.open_entry;r->maintenance.hooks.open_entry=stale_ack_open;}
  execute_round(x,false);
  assert(nart_failed(r)&&x->g->outer.epoch==2&&get(&x->multi.released)==1&&m.catalog.references==4);
  if(kind<2)assert(x->g->f[0].frees==frees&&x->g->f[0].writes==writes);
  else assert(r->refs[0].phase==NOAM_RESTORED&&r->refs[0].pending_item==x->g->f[0].sparse.hardware_item);
  dispose_review(x);
 }
 puts("PASS independent prior bound tag, immutable bound tickets, and PREPARE ACK replays reject round two");
}
int main(void){
 setvbuf(stdout,0,_IONBF,0);candidate_fixture_main();retained_guards();phase_guards();replay_review();timeout_review();
 completion_review(false);completion_review(true);receipt_review();evidence_review();
 puts("PASS independent N43 core review baseline; host mocks, no VMX claim");return 0;
}
