/* Independent host-only privileged-operation failure and reopen probes.
 * The runner creates n42b_fixture_base.inc from the candidate fixture with
 * only its main function renamed. No production source is changed. */
#include "n42b_fixture_base.inc"
static unsigned review_kind;
static uint32_t review_finish_window,review_wait_failed_once;
static bool review_poll(void *p){
 struct shared *g=p;
 if(actual_cpu!=1||!__atomic_load_n(&review_finish_window,__ATOMIC_ACQUIRE))return true;
 if(review_kind==16)return false;
 if(review_kind==17&&!__atomic_exchange_n(&review_wait_failed_once,1,__ATOMIC_ACQ_REL)){
  while(!__atomic_load_n(&g->runtime.released,__ATOMIC_ACQUIRE))sched_yield();
  return false; /* A failed final wait whose recheck must observe commit won. */
 }
 return true;
}
static void review_unlock(void *p){
 struct fixture *f=p;struct nested_atomic_retrack *r=&f->shared->runtime;
 bool final=(review_kind==16||review_kind==17)&&actual_cpu==0&&r->maintenance.state==TAM_OPEN&&r->maintenance.prepared==3&&
  !__atomic_load_n(&r->released,__ATOMIC_ACQUIRE);
 meta_unlock(p);
 if(final){
  __atomic_store_n(&review_finish_window,1,__ATOMIC_RELEASE);
  /* AP's normal healthy wait fails, then its real abort path poisons TAM and
   * dispatch. BSP is paused after TAM's final unlock, before outer commit. */
  if(review_kind==16)while(!__atomic_load_n(&f->shared->dispatch.fault_latch,__ATOMIC_ACQUIRE))sched_yield();
 }
}
static bool (*review_original_open)(void *,struct tam_ticket);
static bool review_load(void *p,uint64_t v){
 struct fixture *f=p;
 if(f->vcpu.owner_apic)return hw_load(p,v);
 bool first=v==f->vcpu.vmcs01;
 if((review_kind==1&&first)||(review_kind==2&&!first))return false;
 bool ok=hw_load(p,v);
 if((review_kind==3&&first)||(review_kind==4&&!first))return false;
 return ok;
}
static bool review_write(void *p,uint64_t v){
 struct fixture *f=p;bool ok=hw_write(p,v);
 if(!f->vcpu.owner_apic&&((review_kind==5&&f->writes==1)||(review_kind==6&&f->writes==2)))return false;
 return ok;
}
static bool review_read(void *p,uint64_t *v){
 struct fixture *f=p;bool ok=hw_read(p,v);
 if(!f->vcpu.owner_apic&&ok&&((review_kind==7&&f->reads==5)||(review_kind==8&&f->reads==9)))*v^=0x40;
 return ok;
}
static bool review_current(void *p,uint64_t *v){
 struct fixture *f=p;
 if(!f->vcpu.owner_apic&&review_kind==9)return false;
 return hw_current(p,v);
}
static bool review_invept(void *p,uint64_t v){
 struct fixture *f=p;bool ok=hw_invept(p,v);
 return ok&&(f->vcpu.owner_apic||review_kind!=10);
}
static bool review_parking(void *p){
 struct fixture *f=p;return (f->vcpu.owner_apic||review_kind!=11)&&parking(p);
}
static bool review_open(void *p,struct tam_ticket t){
 struct nested_atomic_retrack *r=p;
 assert(review_original_open(p,t));
 struct nested_owner_atomic *a=r->refs[0].owner;
 assert(r->maintenance.state==TAM_REOPENING);
 assert(!nested_owner_atomic_entry(a,a->vcpu->vmcs02,*r->refs[0].composed_eptp,true));
 meta_lock(a->config.context);assert(!a->atomic.poisoned);meta_unlock(a->config.context);
 assert(!__atomic_load_n(&r->released,__ATOMIC_ACQUIRE));
 return review_kind!=14;
}
static void review_select(struct fixture *f,struct nested_ept_context_item *item){
 actual_cpu=f->vcpu.owner_apic;struct nested_owner_sparse *s=&f->sparse;
 struct nested_ept_context_item *old=s->hardware_item;
 assert(nested_ept_contexts_select(&s->contexts,item->key)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item *next=nested_ept_contexts_pin_active(&s->contexts);assert(next==item);
 f->physical[1]=f->composed=next->tree.eptp;
 assert(nested_ept_contexts_unpin(&s->contexts,old));s->hardware_item=next;s->selected12=next->key;
 assert(nested_owner_atomic_entry(&f->owner,f->vcpu.vmcs02,f->composed,true));
}
static void review_all_roots(struct shared *g){
 struct nested_atomic_retrack *r=&g->runtime;
 for(unsigned c=0;c<2;c++)for(unsigned n=0;n<2;n++){
  struct fixture *f=&g->f[c];struct tam_root *p=&r->maintenance.roots[c][n];review_select(f,p->item);
  assert(nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));uint64_t leaf=0;
  assert(nested_owner_atomic_item_leaf(&f->owner,p->item,0xf000,&leaf)&&!(leaf&2));
  assert(p->item->tree.generation==p->new_generation&&p->binding->pin.live);
 }
 assert((__atomic_load_n(&ram[18][15],__ATOMIC_ACQUIRE)&0x300)==0x100);
 assert((__atomic_load_n(&ram[26][15],__ATOMIC_ACQUIRE)&0x300)==0x100);
 for(unsigned c=0;c<2;c++)for(unsigned n=0;n<2;n++){
  struct fixture *f=&g->f[c];struct tam_root *p=&r->maintenance.roots[c][n];review_select(f,p->item);uint64_t leaf=0;
  assert(nested_owner_atomic_item_leaf(&f->owner,p->item,0xf000,&leaf)&&!(leaf&2));
  assert(nested_owner_atomic_fault(&f->owner,0xf008,0x18a,0));
  assert(nested_owner_atomic_item_leaf(&f->owner,p->item,0xf000,&leaf)&&(leaf&2));
 }
 assert((__atomic_load_n(&ram[18][15],__ATOMIC_ACQUIRE)&0x300)==0x300);
 assert((__atomic_load_n(&ram[26][15],__ATOMIC_ACQUIRE)&0x300)==0x300);
 for(unsigned c=0;c<2;c++){
  struct nested_owner_atomic *a=&g->f[c].owner;
  assert(a->target_promotions[0]==(c?1:2)&&a->target_promotions[1]==2&&a->target_promotions[2]==(c?2:0));
 }
 assert(m.catalog.references==4);
 puts("PASS N42B all four production read retracks and new-write promotions; host fault requests, no executed guest instructions");
}
static void review_failure(unsigned kind){
 review_kind=kind;__atomic_store_n(&review_finish_window,0,__ATOMIC_RELAXED);__atomic_store_n(&review_wait_failed_once,0,__ATOMIC_RELAXED);
 struct shared *g=make();
 if(kind==16||kind==17){for(unsigned c=0;c<2;c++)g->f[c].owner.config.unlock=review_unlock;g->runtime.poll=review_poll;g->runtime.context=g;}
 boot(g);
 struct nested_atomic_retrack *r=&g->runtime;struct fixture *f=&g->f[0];
 struct nested_ept_context_item *active=f->sparse.hardware_item;
 unsigned before_free=f->frees;uint64_t original=active->tree.eptp;
 r->refs[0].ops=(struct noam_primitives){review_current,review_load,review_read,review_write,review_invept,review_parking};
 if(kind==12)f->owner.bindings[1].pin.live=false;
 if(kind==13)active->pins=0;
 if(kind==14||kind==15){review_original_open=r->maintenance.hooks.open_entry;r->maintenance.hooks.open_entry=review_open;}
 pthread_t threads[2];void *results[2];
 for(unsigned c=0;c<2;c++)assert(!pthread_create(&threads[c],0,round_thread,&g->f[c]));
 for(unsigned c=0;c<2;c++)assert(!pthread_join(threads[c],&results[c]));
 if(kind==15||kind==17){assert(results[0]&&results[1]&&!nart_failed(r)&&g->outer.epoch==2);if(kind==15)review_all_roots(g);if(kind==17)assert(review_wait_failed_once);}
 else{
  fprintf(stderr,"REVIEW kind=%u results=%lu,%lu fault=%u state=%u outer=%lu released=%u\n",kind,(unsigned long)results[0],(unsigned long)results[1],r->fault,r->maintenance.state,(unsigned long)g->outer.epoch,r->released);
  assert(!results[0]&&!results[1]&&nart_failed(r)&&r->maintenance.state==TAM_POISONED);
  assert(g->outer.epoch==1&&m.catalog.references==4&&f->sparse.pool.live);
  assert(!__atomic_load_n(&r->released,__ATOMIC_ACQUIRE));
  assert(g->dispatch.domain.gate==NESTED_DOMAIN_POISON);
  if(kind<=5||kind==7||kind==9||kind==10||kind==11||kind==12||kind==13)assert(f->frees==before_free);
  if(kind==5||kind==7){assert(f->physical[1]==f->fixture.eptp&&f->sparse.hardware_item==active&&active->pins==1);}
  if(kind==6||kind==8){assert(f->physical[1]==original&&!f->sparse.hardware_item&&r->refs[0].pending_item==active&&active->pins==1);}
  if(kind==14)assert(f->physical[1]==original&&f->sparse.hardware_item==active&&active->pins==1);
 }
 for(unsigned c=0;c<2;c++)assert(g->f[c].launched[0]&&g->f[c].launched[1]&&g->f[c].vcpu.launched01&&g->f[c].vcpu.launched02);
 printf("PASS N42B independent operation/reopen case %u\n",kind);free(g);
}
int main(void){
 n42b_fixture_main();
 for(unsigned kind=1;kind<=17;kind++)review_failure(kind);
 return 0;
}
