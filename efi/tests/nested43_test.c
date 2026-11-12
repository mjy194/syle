/* N43 production host fixture. The included N42B fixture only substitutes
 * physical allocation/VMCS/INVEPT primitives; all lifecycle code is production. */
#include "n43_fixture_base.inc"
#include "nested_atomic_multiround.h"
struct multifixture {
 struct shared *g;struct nested_atomic_multiround multi;
 uint64_t generation;unsigned mode,cas_count;bool stale_checked[2];
 uint32_t finish_window,wait_failed,completion_window,completion_resume,returned[2];
 struct tam_ticket old_ticket;struct nested_domain_maintenance_ticket old_domain;
 struct nested_owner_atomic_maintenance old_refs[2];
 struct tam_root old_roots[2][2];struct tam_flush_receipt old_receipts[2][2];
};
static struct multifixture *current_test;
static uint64_t get(const uint64_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static uint32_t get32(const uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
static void select_item(struct fixture *f,struct nested_ept_context_item *item){
 actual_cpu=f->vcpu.owner_apic;struct nested_owner_sparse *s=&f->sparse;
 struct nested_ept_context_item *old=s->hardware_item;
 assert(nested_ept_contexts_select(&s->contexts,item->key)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item *next=nested_ept_contexts_pin_active(&s->contexts);assert(next==item);
 f->physical[1]=f->composed=next->tree.eptp;
 assert(nested_ept_contexts_unpin(&s->contexts,old));s->hardware_item=next;s->selected12=next->key;
 assert(nested_owner_atomic_entry(&f->owner,f->vcpu.vmcs02,f->composed,true));
}
static unsigned trace_count(struct fixture *f){unsigned n=0;for(unsigned k=0;k<NESTED_ATOMIC_TRACES;k++)n+=f->owner.traces[k].used;return n;}
static void all_roots(struct multifixture *x,unsigned round){
 struct shared *g=x->g;struct nested_atomic_retrack *r=&g->runtime;
 unsigned before[2]={trace_count(&g->f[0]),trace_count(&g->f[1])};
 for(unsigned c=0;c<2;c++)for(unsigned n=0;n<2;n++){
  struct fixture *f=&g->f[c];struct tam_root *p=&r->maintenance.roots[c][n];select_item(f,p->item);
  assert(nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));uint64_t leaf=0;
  assert(nested_owner_atomic_item_leaf(&f->owner,p->item,0xf000,&leaf)&&!(leaf&2));
  assert(p->item->tree.generation==p->new_generation&&p->binding->pin.live);
 }
 assert((__atomic_load_n(&ram[18][15],__ATOMIC_ACQUIRE)&0x300)==0x100);
 assert((__atomic_load_n(&ram[26][15],__ATOMIC_ACQUIRE)&0x300)==0x100);
 for(unsigned c=0;c<2;c++)for(unsigned n=0;n<2;n++){
  struct fixture *f=&g->f[c];struct tam_root *p=&r->maintenance.roots[c][n];select_item(f,p->item);uint64_t leaf=0;
  assert(nested_owner_atomic_item_leaf(&f->owner,p->item,0xf000,&leaf)&&!(leaf&2));
  assert(nested_owner_atomic_fault(&f->owner,0xf008,0x18a,0));
  assert(nested_owner_atomic_item_leaf(&f->owner,p->item,0xf000,&leaf)&&(leaf&2));
 }
 for(unsigned c=0;c<2;c++){
  struct fixture *f=&g->f[c];struct nested_owner_atomic *a=&f->owner;
  assert(a->target_promotions[0]==(c?1:1+round)&&a->target_promotions[1]==1+round&&a->target_promotions[2]==(c?1+round:0));
  assert(trace_count(f)==before[c]+2);
  for(struct nested_ept_context_item *item=f->sparse.contexts.items;item;item=item->next)if(item->key==0x1405e)select_item(f,item);
 }
 assert(m.catalog.references==4);
}
static void *multi_thread(void *p){
 struct fixture *f=p;actual_cpu=f->vcpu.owner_apic;struct multifixture *x=current_test;
 bool ok=namr_round(&x->multi,actual_cpu,x->generation);
 __atomic_store_n(&x->returned[actual_cpu],1,__ATOMIC_RELEASE);return (void *)(uintptr_t)ok;
}
static void execute_round(struct multifixture *x,bool expected){
 current_test=x;pthread_t t[2];void *results[2];
 for(unsigned c=0;c<2;c++)__atomic_store_n(&x->returned[c],0,__ATOMIC_RELAXED);
 assert(!pthread_create(&t[0],0,multi_thread,&x->g->f[0]));
 while(get(&x->multi.arrived[0])<x->generation&&!get32(&x->returned[0]))sched_yield();
 if(!get32(&x->returned[0])){
  actual_cpu=1;
  assert(x->g->runtime.maintenance.state==TAM_OPEN&&x->g->dispatch.domain.gate==NESTED_DOMAIN_OPEN);
  assert(nested_owner_atomic_entry(&x->g->f[1].owner,x->g->f[1].vcpu.vmcs02,x->g->f[1].composed,true));
 }
 assert(!pthread_create(&t[1],0,multi_thread,&x->g->f[1]));
 for(unsigned c=0;c<2;c++)assert(!pthread_join(t[c],&results[c]));
 if(expected)assert(results[0]&&results[1]);else assert(!results[0]&&!results[1]);
}
static struct multifixture *create_multi(void){
 struct multifixture *x=calloc(1,sizeof *x);assert(x);x->g=make();x->multi.runtime=&x->g->runtime;x->generation=1;
 for(unsigned c=0;c<2;c++){x->g->f[c].owner.empty_ready=namr_empty_ready;x->g->f[c].owner.empty_context=&x->multi;}
 boot(x->g);
 for(struct nested_ept_context_item *item=x->g->f[0].sparse.contexts.items;item;item=item->next)
  if(item->key==0x1005e)select_item(&x->g->f[0],item);
 return x;
}
static void snapshot_first(struct multifixture *x){
 struct nested_atomic_retrack *r=&x->g->runtime;x->old_ticket=r->ticket;x->old_domain=r->domain_ticket;
 for(unsigned c=0;c<2;c++)x->old_refs[c]=r->refs[c];
 for(unsigned c=0;c<2;c++)for(unsigned n=0;n<2;n++){x->old_roots[c][n]=r->maintenance.roots[c][n];x->old_receipts[c][n]=r->maintenance.receipts[c][n];}
}
static bool (*original_open)(void *,struct tam_ticket);
static bool check_open(void *p,struct tam_ticket t){
 struct nested_atomic_multiround *multi=p;struct multifixture *x=current_test;struct nested_atomic_retrack *r=multi->runtime;
 assert(get(&multi->released)==x->generation-1);
 assert(r->outer_stamp->epoch==x->generation);
 if(x->mode==15)return false;
 return original_open(p,t);
}
static bool multi_poll(void *p){
 struct multifixture *x=p;
 if(actual_cpu!=1||!get32(&x->finish_window))return true;
 if(x->mode==16)return false;
 if(x->mode==17&&!__atomic_exchange_n(&x->wait_failed,1,__ATOMIC_ACQ_REL)){
  while(get(&x->multi.released)!=2)sched_yield();
  return false;
 }
 return true;
}
static void multi_unlock(void *p){
 struct fixture *f=p;struct multifixture *x=current_test;struct nested_atomic_retrack *r=&f->shared->runtime;
 bool window=x&&x->generation==2&&(x->mode==16||x->mode==17)&&actual_cpu==0&&
  r->maintenance.state==TAM_OPEN&&r->maintenance.prepared==3&&r->ticket.nonce==2&&get(&x->multi.released)==1;
 meta_unlock(p);
 if(window){__atomic_store_n(&x->finish_window,1,__ATOMIC_RELEASE);
  if(x->mode==16)while(!get32(&f->shared->dispatch.fault_latch))sched_yield();}
}
static bool uncertain_write(void *p,uint64_t value){
 struct fixture *f=p;bool ok=hw_write(p,value);struct multifixture *x=current_test;
 return ok&&!(!f->vcpu.owner_apic&&((x->mode==18&&f->writes==3)||(x->mode==19&&f->writes==4)));
}
static uint64_t clear_load(void *p,uint64_t *q){(void)p;return __atomic_load_n(q,__ATOMIC_ACQUIRE);}
static bool clear_cas(void *p,uint64_t *q,uint64_t before,uint64_t after){
 (void)p;struct multifixture *x=current_test;
 if(++x->cas_count==2)__atomic_fetch_xor(q,UINT64_C(0x400000),__ATOMIC_ACQ_REL);
 return __atomic_compare_exchange_n(q,&before,after,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static bool replay_current(void *p,uint64_t *value){
 struct fixture *f=p;struct multifixture *x=current_test;struct nested_atomic_retrack *r=&x->g->runtime;
 if(x->generation==2&&r->refs[actual_cpu].phase==NOAM_BOUND&&!x->stale_checked[actual_cpu]){
  x->stale_checked[actual_cpu]=true;
  struct tam_ticket old=x->old_ticket;unsigned c=actual_cpu;
  assert(tam_drain(&r->maintenance,old)==TAM_DRAIN_FAILED);
  assert(!tam_revoke(&r->maintenance,c,old)&&!tam_clear(&r->maintenance,old)&&!tam_reset(&r->maintenance,c,old));
  assert(!tam_prepare(&r->maintenance,c,old)&&!tam_finish(&r->maintenance,old));
  assert(!noam_stopped(&r->refs[c],old)&&!noam_remove(&r->refs[c],old)&&!noam_restore(&r->refs[c],old,&x->old_roots[c][0]));
  struct tam_flush_receipt out={0};assert(!noam_flush(&r->refs[c],old,&x->old_roots[c][0],&out));
  assert(!noam_commit(&r->refs[c],old,x->old_refs[c].committed_stamp));
  assert(!nested_domain_dispatch_maintenance_stopped(&x->g->dispatch,c,f->owner.token,x->old_domain));
  assert(!nested_domain_dispatch_maintenance_ack(&x->g->dispatch,c,x->old_domain));
  assert(!nested_domain_dispatch_maintenance_publish(&x->g->dispatch,x->old_domain));
  assert(!nested_domain_dispatch_maintenance_complete(&x->g->dispatch,x->old_domain));
  for(unsigned phase=NOAM_FRESH;phase<=NOAM_UNCERTAIN;phase++)if(phase!=NOAM_COMMITTED){
   struct nested_owner_atomic_maintenance copy=x->old_refs[c];copy.phase=phase;
   assert(!noam_bind(&copy,r->ticket,r->domain_ticket));
  }
  assert(r->maintenance.state==TAM_STOPPING&&!nart_failed(r));
 }
 return hw_current(p,value);
}
static bool (*original_flush)(void *,unsigned,struct tam_ticket,const struct tam_root *,struct tam_flush_receipt *);
static bool stale_receipt(void *p,unsigned c,struct tam_ticket t,const struct tam_root *root,struct tam_flush_receipt *out){
 if(!original_flush(p,c,t,root,out))return false;
 if(!c){
  struct multifixture *x=current_test;struct fixture *f=&x->g->f[c];
  for(unsigned n=0;n<2;n++)if(root->item==x->old_roots[c][n].item)*out=x->old_receipts[c][n];
  f->failed_item=root->item;f->failed_generation=root->item->tree.generation;f->failed_frees=f->frees;
 }
 return true;
}
static void second_case(unsigned mode){
 struct multifixture *x=create_multi();current_test=x;execute_round(x,true);snapshot_first(x);all_roots(x,1);
 struct shared *g=x->g;struct nested_atomic_retrack *r=&g->runtime;struct fixture *f=&g->f[0];
 struct nested_ept_context_item *active=f->sparse.hardware_item;uint64_t eptp=active->tree.eptp;unsigned frees=f->frees;
 assert(r->refs[0].phase==NOAM_COMMITTED&&r->refs[1].phase==NOAM_COMMITTED&&!r->refs[0].pending_item&&!r->refs[1].pending_item);
 assert(get(&x->multi.completed[0])==1&&get(&x->multi.completed[1])==1&&g->outer.epoch==2);
 actual_cpu=0;assert(!namr_round(&x->multi,0,1)&&!namr_round(&x->multi,0,3)&&!namr_round(&x->multi,0,0)&&!namr_round(&x->multi,0,NAMR_MAX_GENERATION+1));
 assert(!nart_failed(r)&&get(&x->multi.active)==1);
 x->generation=2;x->mode=mode;
 if(mode==1)f->current=f->vcpu.vmcs01;
 if(mode==2)f->physical[0]=eptp;
 if(mode==3)f->physical[1]^=0x40;
 if(mode==4)f->fail_write=f->writes+1;
 if(mode==5)f->fail_read=f->reads+5;
 if(mode==6)f->fail_write=f->writes+2;
 if(mode==7)f->fail_read=f->reads+9;
 if(mode==8)f->fail_load=f->loads+1;
 if(mode==9)f->fail_load=f->loads+2;
 if(mode==10)f->fail_invept_eptp=eptp;
 if(mode==11)for(struct nested_ept_context_item *it=f->sparse.contexts.items;it;it=it->next)if(it!=active)f->fail_invept_eptp=it->tree.eptp;
 if(mode==12)f->bad_read=f->reads+5;
 if(mode==13)f->bad_read=f->reads+9;
 if(mode==14){f->owner.bindings[1].pin.live=false;}
 if(mode==15||mode==0){original_open=r->maintenance.hooks.open_entry;r->maintenance.hooks.open_entry=check_open;}
 if(mode==16||mode==17){for(unsigned c=0;c<2;c++)g->f[c].owner.atomic.config.unlock=multi_unlock;r->poll=multi_poll;r->context=x;}
 if(mode==18||mode==19)r->refs[0].ops.write_eptp=uncertain_write;
 if(mode==20){r->maintenance.hooks.load=clear_load;r->maintenance.hooks.cas=clear_cas;}
 if(mode==21)f->vcpu.vmcs01+=0x10000;
 if(mode==22)f->launched[1]=false;
 if(mode==23)f->vcpu.launched02=false;
 if(mode==24)r->maintenance.next_nonce=UINT64_MAX;
 if(mode==25)g->dispatch.domain.maintenance_epoch=UINT64_MAX;
 if(mode==27){original_flush=r->maintenance.hooks.flush;r->maintenance.hooks.flush=stale_receipt;}
 if(mode==28)for(unsigned c=0;c<2;c++)r->refs[c].ops.current=replay_current;
 if(mode==26)f->fail_read=f->reads+10; /* Commit verification is post-release. */
 execute_round(x,mode==0||mode==17||mode==28);
 if(mode==0||mode==17||mode==28){
  assert(!nart_failed(r)&&get(&x->multi.released)==2&&get(&x->multi.completed[0])==2&&get(&x->multi.completed[1])==2&&g->outer.epoch==3);
  assert(r->ticket.nonce==x->old_ticket.nonce+1&&r->domain_ticket.epoch==x->old_domain.epoch+1);
  assert(namr_release_timeout(&x->multi,1)&&!nart_failed(r));
  for(unsigned c=0;c<2;c++)for(unsigned n=0;n<2;n++){
   struct tam_root *p=&r->maintenance.roots[c][n],*old=&x->old_roots[c][n];
   assert(p->item==old->item&&p->full_eptp==old->full_eptp&&p->identity.allocator_lifetime==old->identity.allocator_lifetime);
   assert(p->old_generation==old->new_generation&&p->new_generation==p->old_generation+1);
   assert(!noam_ticket_equal(r->maintenance.receipts[c][n].ticket,x->old_receipts[c][n].ticket));
   assert(g->f[c].invepts==4&&p->binding->pin.live&&r->refs[c].phase==NOAM_COMMITTED&&!r->refs[c].pending_item);
  }
  assert(m.catalog.references==4);all_roots(x,2);
  assert(g->f[0].owner.used==2&&g->f[1].owner.used==3&&g->f[1].provider_flushes==1);
  printf("TRACE_USAGE cpu0=%u cpu1=%u capacity=%u\n",trace_count(&g->f[0]),trace_count(&g->f[1]),NESTED_ATOMIC_TRACES);
 }else{
  assert(nart_failed(r)&&r->maintenance.state==TAM_POISONED&&g->dispatch.domain.gate==NESTED_DOMAIN_POISON);
  assert(g->outer.epoch==(mode==26?3:2)&&get(&x->multi.released)==(mode==26?2:1));
  actual_cpu=0;assert(!namr_round(&x->multi,0,3));actual_cpu=1;assert(!namr_round(&x->multi,1,3));
  assert(f->sparse.pool.live&&m.catalog.references==4);
  if(mode==4||mode==5||mode==12||mode==18){assert(active->pins==1&&f->sparse.hardware_item==active&&!r->refs[0].pending_item);assert(f->frees==frees);}
  if(mode==6||mode==7||mode==13||mode==19){assert(active->pins==1&&!f->sparse.hardware_item&&r->refs[0].pending_item==active);}
  if(mode==18)assert(f->physical[1]==f->fixture.eptp);
  if(mode==19)assert(f->physical[1]==eptp);
  if(mode==10||mode==11||mode==27)assert(f->failed_item&&f->failed_item->tree.generation==f->failed_generation&&f->frees==f->failed_frees);
  if(mode==20)assert(r->maintenance.changed==1);
  if(mode==26)assert(active->pins==1&&f->sparse.hardware_item==active&&r->refs[0].pending_item==active&&r->refs[0].phase==NOAM_UNCERTAIN);
 }
 printf("PASS N43 second-round case %u\n",mode);free(g);free(x);current_test=0;
}
static bool hold_completion(void *p,uint64_t *value){
 struct fixture *f=p;struct multifixture *x=current_test;
 struct nested_owner_atomic_maintenance *ref=&x->g->runtime.refs[1];
 if(actual_cpu==1&&ref->phase==NOAM_RESTORED&&get(&x->multi.released)==x->generation){
  __atomic_store_n(&x->completion_window,1,__ATOMIC_RELEASE);
  while(!get32(&x->completion_resume))sched_yield();
 }
 return hw_read(f,value);
}
static void completion_gate(void){
 struct multifixture *x=create_multi();current_test=x;
 x->g->runtime.refs[1].ops.read_eptp=hold_completion;
 pthread_t t[2];void *result[2];for(unsigned c=0;c<2;c++)assert(!pthread_create(&t[c],0,multi_thread,&x->g->f[c]));
 while(!get32(&x->completion_window)||get(&x->multi.completed[0])!=1)sched_yield();
 assert(!get32(&x->returned[0])&&!get32(&x->returned[1]));
 actual_cpu=0;assert(!namr_round(&x->multi,0,2));
 assert(get(&x->multi.active)==1&&get(&x->multi.claimed[0])==1&&x->g->runtime.ticket.nonce==1);
 assert(namr_release_timeout(&x->multi,1)&&!nart_failed(&x->g->runtime));
 __atomic_store_n(&x->completion_resume,1,__ATOMIC_RELEASE);
 for(unsigned c=0;c<2;c++){assert(!pthread_join(t[c],&result[c]));assert(result[c]);}
 for(unsigned kind=0;kind<7;kind++){
  struct tr_atomic_maintenance *m=&x->g->runtime.maintenance;
  if(kind==0)m->owner_busy[0]=true;
  if(kind==1)m->owner_busy[1]=true;
  if(kind==2)m->operations[0].live=true;
  if(kind==3)m->operations[1].live=true;
  if(kind==4)m->operation_nonce[0]=1;
  if(kind==5)m->operation_nonce[1]=1;
  if(kind==6)__atomic_store_n(&x->multi.completed[1],0,__ATOMIC_RELEASE);
  assert(!namr_round(&x->multi,0,2));assert(get(&x->multi.active)==1&&get(&x->multi.claimed[0])==1&&!nart_failed(&x->g->runtime));
  m->owner_busy[0]=m->owner_busy[1]=false;m->operations[0].live=m->operations[1].live=false;
  m->operation_nonce[0]=m->operation_nonce[1]=0;__atomic_store_n(&x->multi.completed[1],1,__ATOMIC_RELEASE);
 }
 free(x->g);free(x);current_test=0;puts("PASS N43 delayed prior completion and retained-operation reuse guards");
}
static void fill_owner(struct fixture *f){
 for(struct nested_ept_context_item *item=f->sparse.contexts.items;item;item=item->next){
  select_item(f,item);
  for(unsigned n=1;n<=32;n++){
   uint64_t leaf=0;bool present=nested_owner_atomic_item_leaf(&f->owner,item,(uint64_t)n<<12,&leaf);
   if(!present){bool ok=nested_owner_atomic_fault(&f->owner,(uint64_t)n<<12,0x181,0);
    if(!ok)fprintf(stderr,"exhaust fill failed page=%u traces=%u stage=%u status=%d\n",n,trace_count(f),f->owner.rejection.stage,f->owner.rejection.bridge_status);
    assert(ok);
   }
  }
 }
}
static void exhaust_traces(void){
 struct multifixture *x=create_multi();current_test=x;struct fixture *f=&x->g->f[0];
 /* Separate host resource stress, not the two-round hardware profile. Keep
  * the actual 96-page pool; 64 leaves fit, 126 would fail pool allocation. */
 fill_owner(f);assert(trace_count(f)==64);
 struct nac_trace old=f->owner.traces[0].trace;
 for(unsigned round=1;round<=3;round++){
  x->generation=round;execute_round(x,true);fill_owner(f);assert(trace_count(f)==64*(round+1));
 }
 actual_cpu=0;assert(!f->owner.traces[0].registered&&!f->owner.traces[0].usable);
 assert(!memcmp(&old,&f->owner.traces[0].trace,sizeof old));
 unsigned pins=f->sparse.hardware_item->pins;
 assert(!nested_owner_atomic_fault(&f->owner,0x21000,0x181,0));
 assert(f->owner.failed&&f->owner.atomic.poisoned&&trace_count(f)==256&&f->sparse.hardware_item->pins==pins&&m.catalog.references==4);
 assert(!namr_round(&x->multi,0,4));
 free(x->g);free(x);current_test=0;puts("PASS N43 actual 256-record exhaustion across three host stress clears; no stale record recycling");
}
int main(void){setvbuf(stdout,0,_IONBF,0);for(unsigned n=0;n<=28;n++)second_case(n);completion_gate();exhaust_traces();return 0;}
