/* Reuse the independently exercised real-owner fixture and privileged mocks.
 * Both mutations below execute the production nsrm planner/CAS, not a model. */
#define main n47b_historical_test_main
#include "n47b_owner_test.inc"
#undef main
#include "nested_source_replace_chain.h"
static uint32_t pause_return,returned_pause,allow_return;
bool __real_nsrr_run(struct nested_source_replace_round *,unsigned);
bool __wrap_nsrr_run(struct nested_source_replace_round *r,unsigned c){
 bool ok=__real_nsrr_run(r,c);
 if(ok&&c==1&&__atomic_load_n(&pause_return,__ATOMIC_ACQUIRE)){
  __atomic_store_n(&returned_pause,1,__ATOMIC_RELEASE);
  while(!__atomic_load_n(&allow_return,__ATOMIC_ACQUIRE))sched_yield();
 }
 return ok;
}
static uint32_t pause_walk,walk_paused,walk_continue;
struct nested_ept_atomic_result __real_nac_walk(struct nac_permit *,struct nac_ticket,uint64_t,uint8_t,struct nac_trace *);
struct nested_ept_atomic_result __wrap_nac_walk(struct nac_permit *p,struct nac_ticket t,uint64_t gpa,uint8_t need,struct nac_trace *trace){
 if(tls_cpu==0&&__atomic_load_n(&pause_walk,__ATOMIC_ACQUIRE)){
  __atomic_store_n(&walk_paused,1,__ATOMIC_RELEASE);
  while(!__atomic_load_n(&walk_continue,__ATOMIC_ACQUIRE))sched_yield();
 }
 return __real_nac_walk(p,t,gpa,need,trace);
}
struct chain_fixture {
 struct replacement_fixture *first;
 struct nested_source_replace_round second;
 struct nested_source_replace_chain chain;
 struct nested_root_catalog last;struct ad_updater_gate last_gate;
 struct nrc_span last_spans[12];struct nrc_root last_roots[3];struct nsrm_plan second_plan;
};
static bool manage_second(void *p,uint64_t ticket,const struct nrc_span *s,uint64_t gpa,struct nsrm_authority_page *a){
 struct chain_fixture *f=p;
 assert(!tls_depth&&ticket==f->second.ticket.nonce&&!f->first->next.references&&f->first->next.closed&&
  !f->first->gate.references&&f->first->gate.closed);
 *a=(struct nsrm_authority_page){s->identity,ticket,gpa,s->object,s->lifetime,s->hpa+(gpa-s->gpa),
  s->table_view?(uint64_t *)s->table_view:NULL,s->permissions,6,true,true,true,true,true};return true;
}
static bool change_second(void *p,struct tsr_ticket t,struct nested_root_catalog *old,struct ad_updater_gate *gate,
 struct nested_root_catalog **out,struct ad_updater_gate **gout,void *contexts[2]){
 struct chain_fixture *f=p;struct replacement_fixture *a=f->first;
 assert(old==&a->next&&gate==&a->gate&&t.nonce==f->second.ticket.nonce);
 struct nsrm_request req={.ticket=t.nonce,.full_eptp=old->roots[0].full_eptp,.l2_gpa=0x7000,
  .table_gpa=gpas[4],.table_hpa=old->spans[4].hpa,.table_object=5,.table_lifetime=54,
  .expected_old=__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE),.new_leaf=0x200073,
  .new_gpa=0x200000,.new_hpa=DATA,.new_object=20,.new_lifetime=80,.index=7};
 assert((req.expected_old&~UINT64_C(0x300))==0x210073);
 assert(nsrm_prepare(&f->second_plan,old,manage_second,f,req)&&nsrm_apply(&f->second_plan));
 struct nrc_identity id={1,2,6,7};struct ad_entry_stamp st={1,2,6,7,7};
 assert(nrc_init_atomic_wb_successor(&f->last,old,old->identity,id,48,f->last_spans,12,f->last_roots,3,authority,64));
 assert(ad_updater_gate_init_successor(&f->last_gate,gate,t.stamp,st,64));
 for(size_t n=0;n<old->span_count;n++){struct nrc_span s=old->spans[n];s.identity=id;assert(nrc_add_atomic_span(&f->last,s,(uint64_t *)s.table_view));}
 for(size_t n=0;n<old->root_count;n++){struct nrc_root r=old->roots[n];r.identity=id;r.generation++;assert(nrc_add_root(&f->last,r));}
 assert(nrc_publish(&f->last));*out=&f->last;*gout=&f->last_gate;
 contexts[0]=&a->f.owners[0];contexts[1]=&a->f.owners[1];return true;
}
static void bank_specs(struct chain_fixture *f,struct nsrc_bank b[3]){
 struct replacement_fixture *a=f->first;
 b[0]=(struct nsrc_bank){&a->f.catalog,&a->f.gate,a->f.spans,12,a->f.roots,3,a->f.catalog.identity,a->f.gate.stamp};
 b[1]=(struct nsrc_bank){&a->next,&a->gate,a->spans,12,a->roots,3,{1,2,4,5},{1,2,4,6,5}};
 b[2]=(struct nsrc_bank){&f->last,&f->last_gate,f->last_spans,12,f->last_roots,3,{1,2,6,7},{1,2,6,7,7}};
}
static struct chain_fixture *make_chain(bool bind){
 struct chain_fixture *f=calloc(1,sizeof *f);assert(f);f->first=make(true);
 f->second.dispatch=&f->first->dispatch;f->second.change=change_second;f->second.change_context=f;
 f->second.healthy=round_health;f->second.fault=round_fault;f->second.context=f->first;
 for(unsigned c=0;c<2;c++)f->second.owners[c]=f->first->round.owners[c];
 if(bind){struct nsrc_bank b[3];bank_specs(f,b);assert(nsrc_init(&f->chain,&f->first->round,&f->second,b));}
 return f;
}
struct chain_arg {struct chain_fixture *f;unsigned cpu;uint64_t generation;bool result;};
static void *chain_thread(void *p){struct chain_arg *a=p;tls_cpu=a->cpu;
 a->result=nsrc_run(&a->f->chain,a->cpu,a->generation);return NULL;}
static void run_generation(struct chain_fixture *f,uint64_t generation,bool expected){
 pthread_t threads[2];struct chain_arg args[2];
 for(unsigned c=0;c<2;c++){args[c]=(struct chain_arg){f,c,generation,false};assert(!pthread_create(&threads[c],0,chain_thread,&args[c]));}
 for(unsigned c=0;c<2;c++){assert(!pthread_join(threads[c],0));if(args[c].result!=expected)fprintf(stderr,"generation %lu cpu %u result %u fault %lu phases %u/%u states %u/%u\n",generation,c,args[c].result,f->chain.fault,f->second.refs[0].phase,f->second.refs[1].phase,f->first->round.replacement.state,f->second.replacement.state);assert(args[c].result==expected);}
}
static void between_rounds(struct chain_fixture *f,bool evict){
 for(unsigned c=0;c<2;c++){
  tls_cpu=c;struct owner *o=&f->first->f.owners[c];
  assert(nsrc_before_entry(&f->chain,c,1));
  struct nested_ept_result result;
  assert(tr_atomic_fault(&o->wrapper.atomic,o->sparse.hardware_item,0x7000,1,&result)==TR_ATOMIC_FILLED&&result.hpa==DATA+0x8000);
  if(evict){
   struct tr_registry_slot *old=f->chain.handoff.departure[c].root.slot;
   assert(nested_ept_contexts_unpin(&o->sparse.contexts,o->sparse.hardware_item));o->sparse.hardware_item=NULL;
   select_root(o,2);read_write(o,true);select_root(o,1-c);read_write(o,true);
   assert(old->directory.forgotten&&!old->directory.item);
   o->sparse.hardware_item=nested_ept_contexts_pin_active(&o->sparse.contexts);assert(o->sparse.hardware_item);
   o->physical[1]=o->composed=o->sparse.hardware_item->tree.eptp;
  }
  /* Model an intervening genuine guest entry/full exit at the dispatch layer.
   * VMX remains injected, so this is not physical runtime evidence. */
  struct nested_domain *d=&f->first->dispatch.domain;struct nested_domain_cpu_token ct={1,2,c};
  assert(nested_domain_prepare(d,c,o->wrapper.token,true)&&nested_domain_start(d,c,o->wrapper.token));
  assert(nested_domain_guest_enter(d,ct)&&nested_domain_guest_leave(d,ct)&&nested_domain_finish(d,c,o->wrapper.token,NESTED_DOMAIN_EXIT));
 }
}
static void two_rounds(bool evict){
 struct chain_fixture *f=make_chain(true);run_generation(f,1,true);
 assert(f->chain.handoff.departed[0]==1&&f->chain.handoff.departed[1]==1);
 struct tr_source_replace history=f->first->round.replacement;
 struct tsr_receipt departures[2]={f->chain.handoff.departure[0],f->chain.handoff.departure[1]};
 uint64_t nonce=history.next_nonce,command=history.next_command;
 between_rounds(f,evict);assert(f->first->next.references>0);
 uint64_t before[9][512];memcpy(before,tables,sizeof before);
 struct nsrm_plan old_plan=f->first->plan;
 run_generation(f,2,true);
 assert(!memcmp(&old_plan,&f->first->plan,sizeof old_plan));
 for(unsigned page=0;page<9;page++)for(unsigned word=0;word<512;word++)
  if(page!=4||word!=7)assert(tables[page][word]==before[page][word]);
 assert(tables[4][7]==0x200073); /* second CAS cleared target A/D only */
 for(unsigned bank=0;bank<3;bank++)for(unsigned i=9;i<12;i++){
  const struct nrc_span *s=&f->chain.banks[bank].catalog->spans[i];
  assert(s->object==f->first->f.spans[i].object&&s->lifetime==f->first->f.spans[i].lifetime&&s->hpa==f->first->f.spans[i].hpa);
 }
 assert(f->chain.active_generation==2&&!f->chain.fault&&f->second.replacement.state==TSR_RELEASED);
 assert(!memcmp(&history,&f->first->round.replacement,sizeof history));
 assert(f->second.replacement.next_nonce>nonce&&f->second.replacement.next_command>command);
 assert(f->first->f.catalog.closed&&f->first->f.catalog.superseded&&f->first->next.closed&&f->first->next.superseded);
 assert(f->first->f.gate.closed&&f->first->f.gate.superseded&&f->first->gate.closed&&f->first->gate.superseded);
 assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&~UINT64_C(0x300))==0x200073);
 assert(f->first->plan.applied&&f->second_plan.applied);
 for(unsigned c=0;c<2;c++){
  tls_cpu=c;struct owner *o=&f->first->f.owners[c];
  assert(nosr_root_equal(departures[c].root,f->chain.handoff.departure[c].root));
  assert(!nsrc_before_entry(&f->chain,c,1)&&!nsrr_before_entry(&f->first->round,c));
  assert(nsrc_before_entry(&f->chain,c,2));
  assert(!nsrr_retain_stop(&f->first->round)&&!f->chain.fault&&!f->first->faulted);
  assert(!tsr_abort(&f->second.replacement,f->first->round.ticket));
  assert(!nosr_commit(&f->second.refs[c],f->first->round.ticket));
  struct nested_ept_result result;assert(tr_atomic_fault(&o->wrapper.atomic,o->sparse.hardware_item,0x7000,1,&result)==TR_ATOMIC_FILLED&&result.hpa==DATA);
  unsigned used=0;for(unsigned n=0;n<NESTED_ATOMIC_SLOTS;n++)if(o->wrapper.slots[n].directory.reserved)used++;
  printf("N48A CAPACITY evict=%u owner=%u slots=%u/%u serial=%lu lifetime=%lu\n",evict,c,used,NESTED_ATOMIC_SLOTS,o->f->serial,active(o)->directory.identity.allocator_lifetime);
 }
 puts(evict?"PASS N48A two real nsrm P-Q-P rounds, historical root evicted before fresh checkpoint":"PASS N48A two real nsrm P-Q-P rounds, retained pins and immutable history");
}
static void ancestors(void){
 for(unsigned mode=0;mode<12;mode++){
  struct chain_fixture *f=make_chain(false);struct nsrc_bank b[3];bank_specs(f,b);
  switch(mode){
  case 0:b[2].catalog=b[0].catalog;break;case 1:b[2].gate=b[0].gate;break;
  case 2:b[2].spans=b[0].spans;break;case 3:b[2].roots=b[0].roots;break;
  case 4:b[2].identity=b[0].identity;break;case 5:b[2].stamp=b[0].stamp;break;
  case 6:b[2].spans=b[0].spans+1;break;case 7:b[2].roots=b[0].roots+1;break;
  case 8:b[2].span_capacity=SIZE_MAX;break;case 9:b[2].root_capacity=SIZE_MAX;break;
  case 10:b[2].stamp.context_id++;break;case 11:b[2].stamp.lifetime++;break;
  }
  assert(!nsrc_init(&f->chain,&f->first->round,&f->second,b));
  assert(!f->first->round.replacement.successor&&!f->chain.active_generation);
 }
 puts("PASS N48A 12 ancestor identity/storage/overflow collisions rejected before reservation");
}
static void departure_order(void){
 struct chain_fixture *f=make_chain(true);pthread_t first[2],second;struct chain_arg args[3];
 __atomic_store_n(&pause_return,1,__ATOMIC_RELEASE);__atomic_store_n(&returned_pause,0,__ATOMIC_RELEASE);__atomic_store_n(&allow_return,0,__ATOMIC_RELEASE);
 for(unsigned c=0;c<2;c++){args[c]=(struct chain_arg){f,c,1,false};assert(!pthread_create(&first[c],0,chain_thread,&args[c]));}
 while(!__atomic_load_n(&returned_pause,__ATOMIC_ACQUIRE))sched_yield();
 assert(!pthread_join(first[0],0)&&args[0].result);
 assert(f->first->round.committed[0]&&f->first->round.committed[1]);
 assert(f->chain.handoff.departed[0]==1&&!f->chain.handoff.departed[1]);
 args[2]=(struct chain_arg){f,0,2,false};assert(!pthread_create(&second,0,chain_thread,&args[2]));
 while(!__atomic_load_n(&f->chain.transition[0],__ATOMIC_ACQUIRE))sched_yield();
 assert(f->chain.active_generation==1&&!f->second.bootstrapped);
 assert(!nsrc_before_entry(&f->chain,0,1));
 __atomic_store_n(&pause_return,0,__ATOMIC_RELEASE);__atomic_store_n(&allow_return,1,__ATOMIC_RELEASE);
 assert(!pthread_join(first[1],0)&&args[1].result);
 tls_cpu=1;assert(nsrc_run(&f->chain,1,2));assert(!pthread_join(second,0)&&args[2].result);
 puts("PASS N48A actual nsrr return barrier: both committed is insufficient, delayed AP departure blocks successor");
}
static void retained_failures(void){
 for(unsigned mode=0;mode<6;mode++){
  struct chain_fixture *f=make_chain(true);run_generation(f,1,true);between_rounds(f,false);
  struct owner *o=&f->first->f.owners[1];
  unsigned frees=o->frees;uint64_t active_eptp=o->composed;
  if(mode==0)o->physical[1]^=0x40;
  if(mode==1)o->vcpu.launched02=false;
  if(mode==2){o->fail_kind=4;o->fail_n=o->calls[3]+2;}
  if(mode==3){o->fail_kind=3;o->fail_n=o->calls[2]+2;}
  if(mode==4)f->first->next.next_nonce=UINT64_MAX-63;
  if(mode==5)f->first->gate.next_nonce=UINT64_MAX-63;
  run_generation(f,2,false);
  assert(f->chain.fault&&f->first->faulted&&f->first->dispatch.domain.gate==NESTED_DOMAIN_POISON);
  assert(!nsrc_before_entry(&f->chain,0,1)&&!nsrc_before_entry(&f->chain,1,2));
  if(mode!=2){assert(f->chain.active_generation==1&&!f->second.bootstrapped);assert(o->frees==frees&&o->sparse.hardware_item->pins==1&&o->composed==active_eptp);}
  else {assert(f->chain.active_generation==2&&f->second.replacement.state==TSR_POISONED);assert(f->second.refs[1].pending_live&&o->sparse.contexts.active->pins==1);}
 }
 puts("PASS N48A current handoff/nonce exhaustion and second-round restore failures latch retain-stop; old failures do not poison new generation");
}
static void budget_preflight(void){
 for(unsigned mode=0;mode<12;mode++){
  struct chain_fixture *f=make_chain(false);struct replacement_fixture *a=f->first;
  struct tr_source_replace *r=&a->round.replacement;struct nsrc_bank b[3];bank_specs(f,b);
  switch(mode){
  case 0:r->next_nonce=UINT64_MAX-1;break;case 1:r->next_command=UINT64_MAX-127;break;
  case 2:a->f.directory.version=UINT64_MAX-511;break;case 3:a->f.directory.next_id=UINT64_MAX-3;break;
  case 4:a->f.directory.next_nonce=UINT64_MAX-127;break;case 5:a->f.lease.version=UINT64_MAX-127;break;
  case 6:a->f.lease.budget=a->f.lease.reserved+3;break;case 7:a->f.catalog.next_nonce=UINT64_MAX-127;break;
  case 8:a->f.gate.next_nonce=UINT64_MAX-127;break;case 9:a->f.roots[0].generation=UINT64_MAX-1;break;
  case 10:a->f.serial=UINT64_MAX-4*TSR_ROOTS+1;break;
  case 11:for(unsigned n=0;n<NESTED_ATOMIC_SLOTS-1;n++)a->f.owners[0].wrapper.slots[n].directory.reserved=true;break;
  }
  struct nested_source_replace_chain zero={0};
  assert(!nsrc_init(&f->chain,&a->round,&f->second,b));
  assert(!memcmp(&zero,&f->chain,sizeof zero)&&!r->successor&&r->state==TSR_OPEN);
 }
 puts("PASS N48A 12 two-round capacity/high-water exhaustion preflights preserve fresh controller");
}
struct fault_arg {struct owner *owner;enum tr_atomic_status status;};
static void *fault_thread(void *p){struct fault_arg *a=p;tls_cpu=0;struct nested_ept_result result;
 a->status=tr_atomic_fault(&a->owner->wrapper.atomic,a->owner->sparse.hardware_item,0x7000,1,&result);return NULL;}
static void three_generation_replay(void){
 struct chain_fixture *f=make_chain(true);struct owner *o=&f->first->f.owners[0];tls_cpu=0;
 struct nac_permit *saved=&o->wrapper.atomic.fault.permit;
 struct nac_ticket old[2]={o->wrapper.atomic.fault.ticket,{0}};
 struct nac_trace trace[2]={0};trace[0]=o->wrapper.traces[0].trace;struct trl_token typed[2];
 lock(o);assert(trl_token_read(&f->first->f.lease,&active(o)->typed,&typed[0]));unlock(o);
 run_generation(f,1,true);between_rounds(f,false);old[1]=o->wrapper.atomic.fault.ticket;
 bool found=false;for(unsigned n=0;n<o->wrapper.atomic.config.trace_count;n++)if(o->wrapper.traces[n].used&&o->wrapper.traces[n].usable){trace[1]=o->wrapper.traces[n].trace;found=true;break;}assert(found);
 lock(o);assert(trl_token_read(&f->first->f.lease,&active(o)->typed,&typed[1]));unlock(o);
 run_generation(f,2,true);
 __atomic_store_n(&pause_walk,1,__ATOMIC_RELEASE);__atomic_store_n(&walk_paused,0,__ATOMIC_RELEASE);__atomic_store_n(&walk_continue,0,__ATOMIC_RELEASE);
 pthread_t thread;struct fault_arg arg={o,0};assert(!pthread_create(&thread,0,fault_thread,&arg));
 while(!__atomic_load_n(&walk_paused,__ATOMIC_ACQUIRE))sched_yield();
 tls_cpu=0;
 assert(saved==&o->wrapper.atomic.fault.permit&&saved->active);
 uint64_t refs=f->last.references,grefs=f->last_gate.references;
 for(unsigned i=0;i<2;i++){
  assert(saved->source.nonce>old[i].source_nonce&&saved->updater.nonce>old[i].updater_nonce);
  assert(!nac_guard(saved,old[i])&&!nac_end(saved,old[i]));
  assert(!nrc_guard(&saved->source,old[i].source_nonce)&&!nrc_end(&saved->source,old[i].source_nonce));
  assert(!ad_updater_gate_guard(&f->last_gate,&saved->updater,f->last_gate.stamp,old[i].updater_nonce));
  assert(!ad_updater_gate_release(&f->last_gate,&saved->updater,f->last_gate.stamp,old[i].updater_nonce));
  assert(!nac_trace_guard(saved,o->wrapper.atomic.fault.ticket,0x7000,&trace[i]));
  lock(o);assert(!trl_admit(&f->first->f.lease,&o->wrapper.atomic.fault.operation,o->wrapper.atomic.fault.nonce,&active(o)->typed,typed[i],TRL_WB,true,true));unlock(o);
 }
 assert(f->last.references==refs&&f->last_gate.references==grefs&&nac_guard(saved,o->wrapper.atomic.fault.ticket));
 __atomic_store_n(&walk_continue,1,__ATOMIC_RELEASE);assert(!pthread_join(thread,0));__atomic_store_n(&pause_walk,0,__ATOMIC_RELEASE);
 assert(arg.status==TR_ATOMIC_FILLED&&!saved->active&&!f->last_gate.references);
 puts("PASS N48A same actual permit/session/updater storage across three generations rejects both ancestors without consuming current authority");
}
int main(void){two_rounds(false);two_rounds(true);ancestors();departure_order();retained_failures();budget_preflight();three_generation_replay();return 0;}
