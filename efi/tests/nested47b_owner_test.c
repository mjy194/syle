/* Production owner adapter/coordinator/dispatch and pool. VMX primitives are
 * deliberately injected; no privileged instruction or hardware claim. */
#include "n47b_owner_fixture.inc"
#include "nested_source_replace_memory.h"
#include "nested_source_replace_round.h"
#include <sched.h>
struct replacement_fixture {
 struct fixture f;
 struct nested_source_replace_round round;
 struct nested_domain_dispatch dispatch;
 struct nested_root_catalog next;struct nrc_span spans[12];struct nrc_root roots[3];struct ad_updater_gate gate;
 struct nsrm_plan plan;
 unsigned validation_fail,identity_fail;uint32_t faulted,plan_paused,plan_continue;
};
static struct replacement_fixture *paused_fixture;
bool __real_nested_domain_dispatch_source_plan(struct nested_domain_dispatch *,struct nested_domain_maintenance_ticket *);
bool __wrap_nested_domain_dispatch_source_plan(struct nested_domain_dispatch *d,struct nested_domain_maintenance_ticket *out){
 bool ok=__real_nested_domain_dispatch_source_plan(d,out);
 struct replacement_fixture *f=__atomic_load_n(&paused_fixture,__ATOMIC_ACQUIRE);
 if(ok&&f&&d==&f->dispatch&&tls_cpu==0){
  __atomic_store_n(&f->plan_paused,1,__ATOMIC_RELEASE);
  while(!__atomic_load_n(&f->plan_continue,__ATOMIC_ACQUIRE))sched_yield();
 }
 return ok;
}
static bool hw_op(struct owner *o,unsigned kind){
 assert(tls_cpu==o->cpu&&!tls_depth);o->calls[kind]++;
 return !(o->fail_kind==kind+1&&o->calls[kind]==o->fail_n);
}
static bool hw_current(void *p,uint64_t *v){struct owner *o=p;if(!hw_op(o,0))return false;*v=o->current;
 if(o->bad_kind==1&&o->calls[0]==o->bad_n)*v^=4096;
 return true;}
static bool hw_load(void *p,uint64_t v){struct owner *o=p;assert(v==o->vcpu.vmcs01||v==o->vcpu.vmcs02);
 if(!hw_op(o,1))return false;
 o->current=v;return true;}
static bool hw_read(void *p,uint64_t *v){struct owner *o=p;if(!hw_op(o,2))return false;
 assert(o->current==o->vcpu.vmcs01||o->current==o->vcpu.vmcs02);*v=o->physical[o->current==o->vcpu.vmcs02];
 if(o->bad_kind==3&&o->calls[2]==o->bad_n)*v^=0x40;
 return true;}
static bool hw_write(void *p,uint64_t v){struct owner *o=p;assert(o->current==o->vcpu.vmcs02);
 bool ok=hw_op(o,3);if(ok||o->uncertain_write)o->physical[1]=v;return ok;}
static bool parking(void *p){return ((struct owner *)p)->parking_ok;}
static bool normal_stopped(void *p){struct owner *o=p;assert(!tls_depth);
 return o->stopped&&nested_domain_dispatch_consumer_stopped(o->wrapper.dispatch,o->cpu,o->wrapper.token);}
static bool splan(void *p,struct tsr_ticket t,struct nested_domain_maintenance_ticket *out){
 struct replacement_fixture *f=p;assert(t.nonce&&!tls_depth);return nested_domain_dispatch_source_plan(&f->dispatch,out);}
static bool sclose(void *p,struct tsr_ticket t){struct replacement_fixture *f=p;struct nested_domain_maintenance_ticket d;
 bool ok=nested_domain_dispatch_maintenance_begin(&f->dispatch,NESTED_DOMAIN_SOURCE_REPLACE,&d);
 struct tsr_ticket actual=t;actual.domain=d;return ok&&nosr_ticket_equal(actual,t);}
static bool sstopped(void *p,unsigned c,struct tsr_ticket t){struct replacement_fixture *f=p;
 return c<2&&nested_domain_dispatch_source_stopped(&f->dispatch,c,f->f.owners[c].wrapper.token,t.domain);}
static bool spark(void *p,unsigned c,struct tsr_ticket t,const struct tsr_root *roots,size_t count,struct tsr_receipt *out){
 struct replacement_fixture *f=p;struct nested_owner_source_replace *r=&f->round.refs[c];
 return nosr_bind(r,t)&&nosr_park(r,t,roots,count,out);}
static bool srestore(void *p,unsigned c,struct tsr_ticket t,const struct tsr_root *root,struct tsr_receipt *out){
 struct replacement_fixture *f=p;
 if(f->identity_fail==c+1){
  /* Poisoned item must never be followed after stable identity rejection. */
  root->slot->directory.identity.allocator_lifetime++;
  root->slot->directory.item=(void *)(uintptr_t)1;
 }
 return nosr_restore(&f->round.refs[c],t,root,out);
}
static bool scomplete(void *p,struct tsr_ticket t){struct replacement_fixture *f=p;
 for(unsigned c=0;c<2;c++){
  tls_cpu=c;if(f->validation_fail==c+1)f->f.owners[c].physical[1]^=0x40;
  if(!nosr_validate(&f->round.refs[c],t,&f->round.validated[c]))return false;
 }
 for(unsigned c=0;c<2;c++)if(!nested_domain_dispatch_maintenance_ack(&f->dispatch,c,t.domain))return false;
 return nested_domain_dispatch_maintenance_publish(&f->dispatch,t.domain)&&nested_domain_dispatch_maintenance_complete(&f->dispatch,t.domain);
}
static bool manage(void *p,uint64_t ticket,const struct nrc_span *s,uint64_t gpa,struct nsrm_authority_page *a){
 struct replacement_fixture *f=p;assert(!tls_depth&&ticket==f->round.ticket.nonce&&!f->f.catalog.references&&f->f.catalog.closed&&!f->f.gate.references&&f->f.gate.closed);
 *a=(struct nsrm_authority_page){s->identity,ticket,gpa,s->object,s->lifetime,s->hpa+(gpa-s->gpa),s->table_view?(uint64_t *)s->table_view:NULL,s->permissions,6,true,true,true,true,true};return true;
}
static bool schange(void *p,struct tsr_ticket t,struct nested_root_catalog *old,struct ad_updater_gate *gate,struct nested_root_catalog **out,struct ad_updater_gate **gout,void *contexts[2]){
 struct replacement_fixture *f=p;assert(old==&f->f.catalog&&gate==&f->f.gate&&t.nonce==f->round.ticket.nonce);
 struct nsrm_request req={.ticket=t.nonce,.full_eptp=f->f.roots[0].full_eptp,.l2_gpa=0x7000,.table_gpa=gpas[4],.table_hpa=f->f.spans[4].hpa,.table_object=5,.table_lifetime=54,.expected_old=__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE),.new_leaf=0x210073,.new_gpa=0x210000,.new_hpa=DATA+0x8000,.new_object=22,.new_lifetime=82,.index=7};
 assert(nsrm_prepare(&f->plan,old,manage,f,req)&&nsrm_apply(&f->plan));
 struct nrc_identity id={1,2,4,5};struct ad_entry_stamp stamp={1,2,4,6,5};
 assert(nrc_init_atomic_wb_successor(&f->next,old,old->identity,id,48,f->spans,12,f->roots,3,authority,64));
 assert(ad_updater_gate_init_successor(&f->gate,gate,t.stamp,stamp,64));
 for(size_t n=0;n<old->span_count;n++){struct nrc_span s=old->spans[n];s.identity=id;assert(nrc_add_atomic_span(&f->next,s,(uint64_t *)s.table_view));}
 for(size_t n=0;n<old->root_count;n++){struct nrc_root r=old->roots[n];r.identity=id;r.generation++;assert(nrc_add_root(&f->next,r));}
 assert(nrc_publish(&f->next));*out=&f->next;*gout=&f->gate;contexts[0]=&f->f.owners[0];contexts[1]=&f->f.owners[1];return true;
}
static bool round_health(void *p){return !__atomic_load_n(&((struct replacement_fixture *)p)->faulted,__ATOMIC_ACQUIRE);}
static void round_fault(void *p){__atomic_store_n(&((struct replacement_fixture *)p)->faulted,1,__ATOMIC_RELEASE);}
struct thread_arg {struct replacement_fixture *f;unsigned cpu;bool result,bootstrap;};
static void *run_thread(void *p){struct thread_arg *a=p;tls_cpu=a->cpu;
 a->result=a->bootstrap?nsrr_empty_ready(&a->f->f.owners[a->cpu].wrapper,&a->f->round):nsrr_run(&a->f->round,a->cpu);return NULL;}
static struct replacement_fixture *make(bool round){
 struct replacement_fixture *f=calloc(1,sizeof *f);assert(f);setup(&f->f);
 assert(nested_domain_dispatch_init(&f->dispatch,1,2));
 struct nested_source_replace_round *r=&f->round;r->dispatch=&f->dispatch;r->change=schange;r->change_context=f;
 r->healthy=round_health;r->fault=round_fault;r->context=f;
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->f.owners[c];struct nested_owner_atomic *a=&o->wrapper;
  assert(nested_vcpu_init(&o->vcpu,c,0x600000+c*8192));assert(nested_vcpu_register02(&o->vcpu,c,0x601000+c*8192));
  o->parking.eptp=0x70001e + c*4096;o->sparse.fixture=&o->parking;o->parking_ok=true;
  a->sparse=&o->sparse;a->vcpu=&o->vcpu;a->dispatch=&f->dispatch;a->token=f->dispatch.tokens[c];a->physical_ad=true;
  a->atomic.config.consumer_stopped=normal_stopped;a->config=a->atomic.config;
  r->owners[c]=a;
 }
 if(round){
  pthread_t threads[2];struct thread_arg args[2];
  for(unsigned c=0;c<2;c++){args[c]=(struct thread_arg){f,c,false,true};assert(!pthread_create(&threads[c],0,run_thread,&args[c]));}
  for(unsigned c=0;c<2;c++){assert(!pthread_join(threads[c],0));assert(args[c].result);}
 }else{
  struct tsr_hooks hooks={splan,sclose,sstopped,spark,srestore,scomplete};
  assert(tsr_init(&r->replacement,&r->owners[0]->atomic,&r->owners[1]->atomic,&r->owners[0]->provider,&r->owners[1]->provider,&r->owners[0]->config,&r->owners[1]->config,&hooks,f));
 }
 for(unsigned c=0;c<2;c++){
  struct owner *o=&f->f.owners[c];struct nested_owner_atomic *a=&o->wrapper;tls_cpu=c;
  struct nested_domain *d=&f->dispatch.domain;struct nested_domain_cpu_token ct={1,2,c};
  assert(nested_domain_vmxon(d,c,c?0x2d000:0xd000)&&nested_domain_load(d,c,a->token));
  assert(nested_domain_prepare(d,c,a->token,false)&&nested_domain_start(d,c,a->token));
  assert(nested_domain_guest_enter(d,ct)&&nested_domain_guest_leave(d,ct)&&nested_domain_finish(d,c,a->token,NESTED_DOMAIN_EXIT));
  select_root(o,c);read_write(o,true);select_root(o,2);read_write(o,true);select_root(o,c);
  o->sparse.hardware_item=nested_ept_contexts_pin_active(&o->sparse.contexts);assert(o->sparse.hardware_item);
  o->physical[0]=o->parking.eptp;o->physical[1]=o->composed=o->sparse.hardware_item->tree.eptp;o->current=o->vcpu.vmcs01;
  assert(tr_atomic_entry(&a->atomic,o->sparse.hardware_item,o->composed));
  o->launched[0]=o->launched[1]=o->vcpu.launched01=o->vcpu.launched02=true;
  o->vcpu.cleared01=o->vcpu.cleared02=false;
  struct nosr_config cfg={a,&r->replacement,{hw_current,hw_load,hw_read,hw_write,parking},o,&o->composed,&o->launched[0],&o->launched[1]};
  assert(nosr_init(&r->refs[c],&cfg));
 }
 return f;
}
static void close_round(struct replacement_fixture *f){assert(tsr_close(&f->round.replacement,&f->round.ticket));assert(tsr_drain(&f->round.replacement,f->round.ticket)==TSR_DRAIN_READY);}
static void retire(struct replacement_fixture *f){close_round(f);for(unsigned c=0;c<2;c++){tls_cpu=c;assert(tsr_retire_owner(&f->round.replacement,c,f->round.ticket));assert(f->f.owners[c].current==f->f.owners[c].vcpu.vmcs01);}}
static void publish(struct replacement_fixture *f){retire(f);assert(tsr_replace(&f->round.replacement,f->round.ticket,schange,f));}
static void prepare(struct replacement_fixture *f){publish(f);for(unsigned c=0;c<2;c++){tls_cpu=c;assert(tsr_prepare_owner(&f->round.replacement,c,f->round.ticket));}}
static void successful(void){
 struct replacement_fixture *f=make(false);uint64_t old_hpa[2],old_life[2];
 for(unsigned c=0;c<2;c++){old_hpa[c]=f->f.owners[c].composed;old_life[c]=active(&f->f.owners[c])->directory.identity.allocator_lifetime;}
 prepare(f);
 for(unsigned c=0;c<2;c++){tls_cpu=c;assert(!nosr_commit(&f->round.refs[c],f->round.ticket));assert(f->round.refs[c].pending_live);}
 assert(tsr_finish(&f->round.replacement,f->round.ticket));
 for(unsigned c=0;c<2;c++){
  tls_cpu=c;struct owner *o=&f->f.owners[c];struct nested_owner_source_replace *r=&f->round.refs[c];
  assert(nosr_commit(r,f->round.ticket)&&!nosr_commit(r,f->round.ticket));
  assert(r->phase==NOSR_COMMITTED&&!r->pending_live&&!r->pending.slot&&o->sparse.hardware_item==o->sparse.contexts.active);
  assert(o->sparse.hardware_item->pins==1&&o->composed==old_hpa[c]&&active(o)->directory.identity.allocator_lifetime>old_life[c]);
  assert(o->current==o->vcpu.vmcs01&&o->physical[0]==o->parking.eptp&&o->physical[1]==o->composed&&o->calls[3]==2);
  assert(o->invepts==2&&r->old.slot->directory.forgotten&&!r->old.slot->directory.item);
  assert(tr_atomic_entry(&o->wrapper.atomic,o->sparse.hardware_item,o->composed));
  struct nested_ept_result result;assert(tr_atomic_fault(&o->wrapper.atomic,o->sparse.hardware_item,0x7000,1,&result)==TR_ATOMIC_FILLED&&result.hpa==DATA+0x8000);
 }
 free(f);puts("PASS N47B real coordinator/owner/dispatch success, same-HPA new lifetime, final validation/commit");
}
static void primitive_failures(void){
 /* Every privileged primitive position in bind+park and restore. */
 unsigned cases=0;
 for(unsigned stage=0;stage<2;stage++)for(unsigned kind=1;kind<=4;kind++){
  const unsigned counts[2][4]={{5,2,5,1},{4,2,4,1}};
  for(unsigned n=1;n<=counts[stage][kind-1];n++)for(unsigned uncertain=0;uncertain<(kind==4?2u:1u);uncertain++){
   struct replacement_fixture *f=make(false);struct owner *o=&f->f.owners[0];
   if(stage)publish(f);else close_round(f);
   unsigned frees=o->frees;struct nested_ept_context_item *old=o->sparse.hardware_item;
   memset(o->calls,0,sizeof o->calls);o->fail_kind=kind;o->fail_n=n;o->uncertain_write=uncertain;tls_cpu=0;
   bool ok=stage?tsr_prepare_owner(&f->round.replacement,0,f->round.ticket):tsr_retire_owner(&f->round.replacement,0,f->round.ticket);
   assert(!ok&&f->round.replacement.state==TSR_POISONED&&f->round.refs[0].phase==NOSR_UNCERTAIN);
   assert(o->frees==frees&&o->sparse.pool.live);
   if(!stage)assert(o->sparse.hardware_item==old&&old->pins==1);
   else {assert(o->sparse.contexts.active->pins==1&&!o->sparse.hardware_item);if(kind!=1&&kind!=3)assert(f->round.refs[0].pending_live);}
   assert(o->launched[0]&&o->launched[1]&&o->vcpu.launched01&&o->vcpu.launched02);
   free(f);cases++;
  }
 }
 printf("PASS N47B %u primitive failure/uncertain-write cases retain pins and launch state\n",cases);
}
static void final_failures(void){
 unsigned cases=0;
 for(unsigned commit=0;commit<2;commit++)for(unsigned kind=1;kind<=3;kind++){
  const unsigned counts[3]={3,2,3};
  for(unsigned n=1;n<=counts[kind-1];n++){
   struct replacement_fixture *f=make(false);prepare(f);
   if(commit)assert(tsr_finish(&f->round.replacement,f->round.ticket));
   struct owner *o=&f->f.owners[0];memset(o->calls,0,sizeof o->calls);o->fail_kind=kind;o->fail_n=n;tls_cpu=0;
   bool ok=commit?nosr_commit(&f->round.refs[0],f->round.ticket):tsr_finish(&f->round.replacement,f->round.ticket);
   assert(!ok&&f->round.refs[0].phase==NOSR_UNCERTAIN&&f->round.refs[0].pending_live&&o->sparse.hardware_item->pins==1);
   assert(f->round.replacement.state==(commit?TSR_RELEASED:TSR_POISONED));
   assert(o->launched[0]&&o->launched[1]&&o->vcpu.launched01&&o->vcpu.launched02);free(f);cases++;
  }
 }
 printf("PASS N47B %u final validation/commit primitive failures retain pending pin\n",cases);
}
static void corruptions(void){
 for(unsigned mode=0;mode<11;mode++){
  struct replacement_fixture *f=make(false);struct owner *o=&f->f.owners[0];bool ok;
  if(mode<7){close_round(f);tls_cpu=0;
   if(mode==0)o->current=o->vcpu.vmcs02;
   if(mode==1)o->physical[0]=o->composed;
   if(mode==2)o->physical[1]^=0x40;
   if(mode==3)o->parking_ok=false;
   if(mode==4)o->bad_kind=3,o->bad_n=4; /* park readback */
   if(mode==5)o->bad_kind=1,o->bad_n=4;
   if(mode==6)o->vcpu.launched02=false;
   ok=tsr_retire_owner(&f->round.replacement,0,f->round.ticket);assert(!o->frees&&o->sparse.hardware_item->pins==1);
  }else if(mode<9){publish(f);tls_cpu=0;
   if(mode==7)f->identity_fail=1;else o->bad_kind=3,o->bad_n=o->calls[2]+3;
   ok=tsr_prepare_owner(&f->round.replacement,0,f->round.ticket);
   assert(!o->sparse.hardware_item);if(mode==8)assert(f->round.refs[0].pending_live);
  }else{prepare(f);if(mode==9)f->validation_fail=1;else o->launched[1]=false;
   ok=tsr_finish(&f->round.replacement,f->round.ticket);assert(f->round.refs[0].pending_live&&o->sparse.hardware_item->pins==1);
  }
  assert(!ok&&f->round.replacement.state==TSR_POISONED&&!tsr_released(&f->round.replacement,f->round.ticket));free(f);
 }
 puts("PASS N47B hidden VMCS references, full EPTP, parking, identity-before-pointer and final launch checks");
}
static struct tsr_ticket altered(struct tsr_ticket t,unsigned n){
 switch(n){case 0:t.nonce++;break;case 1:t.source.domain++;break;case 2:t.source.incarnation++;break;
 case 3:t.source.catalog++;break;case 4:t.source.lifetime++;break;case 5:t.stamp.domain_id++;break;
 case 6:t.stamp.incarnation++;break;case 7:t.stamp.context_id++;break;case 8:t.stamp.epoch++;break;
 case 9:t.stamp.lifetime++;break;case 10:t.domain.domain_id++;break;case 11:t.domain.incarnation++;break;
 case 12:t.domain.epoch++;break;case 13:t.domain.purpose=NESTED_DOMAIN_AD_RESET;break;}return t;
}
static void owner_guards(void){struct replacement_fixture *f=make(false);close_round(f);
 struct nested_owner_source_replace *r=&f->round.refs[0];struct owner *o=&f->f.owners[0];
 for(unsigned n=0;n<14;n++){tls_cpu=0;struct tsr_ticket bad=altered(f->round.ticket,n);assert(!nosr_bind(r,bad)&&!o->calls[0]);}
 tls_cpu=1;assert(!nosr_bind(r,f->round.ticket));tls_cpu=0;assert(nosr_bind(r,f->round.ticket));
 for(unsigned n=0;n<14;n++)assert(!nosr_stopped(r,altered(f->round.ticket,n)));
 tls_cpu=1;assert(!nosr_stopped(r,f->round.ticket));tls_cpu=0;assert(nosr_stopped(r,f->round.ticket));
 o->vcpu.busy=true;assert(!nosr_stopped(r,f->round.ticket));o->vcpu.busy=false;
 o->launched[1]=false;assert(!nosr_stopped(r,f->round.ticket));o->launched[1]=true;
 o->vcpu.cleared02=true;assert(!nosr_stopped(r,f->round.ticket));o->vcpu.cleared02=false;
 f->dispatch.internal_pending[0]=true;assert(!nosr_stopped(r,f->round.ticket));f->dispatch.internal_pending[0]=false;
 assert(!nosr_commit(r,f->round.ticket));free(f);puts("PASS N47B exact 14-field ticket, owner CPU, stopped/internal/launch guards");}
static void root_fields(void){
 struct replacement_fixture *f=make(false);prepare(f);struct tsr_root root=f->round.refs[0].pending;
 for(unsigned n=0;n<23;n++){
  struct tsr_root bad=root;
  switch(n){case 0:bad.slot=NULL;break;case 1:bad.identity.domain++;break;case 2:bad.identity.incarnation++;break;
  case 3:bad.identity.root_id++;break;case 4:bad.identity.root_incarnation++;break;case 5:bad.identity.allocator_lifetime++;break;
  case 6:bad.identity.owner++;break;case 7:bad.source.identity.domain++;break;case 8:bad.source.identity.incarnation++;break;
  case 9:bad.source.identity.catalog++;break;case 10:bad.source.identity.lifetime++;break;case 11:bad.source.source++;break;
  case 12:bad.source.owner++;break;case 13:bad.source.generation++;break;case 14:bad.source.lifetime++;break;
  case 15:bad.source.full_eptp++;break;case 16:bad.source.root_gpa++;break;case 17:bad.key++;break;case 18:bad.eptp++;break;
  case 19:bad.hpa++;break;case 20:bad.generation++;break;case 21:bad.history++;break;case 22:bad.pins++;break;}
  assert(!nosr_root_equal(root,bad));
 }
 free(f);puts("PASS N47B all 23 stable-root fields participate in receipt identity");
}
static void first_command_abort(void){
 struct replacement_fixture *f=make(true);pthread_t threads[2];struct thread_arg args[2];
 __atomic_store_n(&paused_fixture,f,__ATOMIC_RELEASE);
 for(unsigned c=0;c<2;c++){args[c]=(struct thread_arg){f,c,false,false};assert(!pthread_create(&threads[c],0,run_thread,&args[c]));}
 while(!__atomic_load_n(&f->plan_paused,__ATOMIC_ACQUIRE))sched_yield();
 /* AP has no acquired command and the public output ticket is not written.
  * Only the coordinator's metadata-locked ticket can safely authorize abort. */
 round_fault(f);assert(!pthread_join(threads[1],0));assert(!args[1].result);
 assert(f->round.replacement.state==TSR_POISONED&&!f->round.ticket.nonce);
 __atomic_store_n(&f->plan_continue,1,__ATOMIC_RELEASE);
 assert(!pthread_join(threads[0],0));assert(!args[0].result);
 __atomic_store_n(&paused_fixture,NULL,__ATOMIC_RELEASE);
 for(unsigned c=0;c<2;c++){assert(!f->f.owners[c].calls[3]&&!f->f.owners[c].frees);assert(f->f.owners[c].sparse.hardware_item->pins==1);}
 free(f);puts("PASS N47B first-command AP failure while BSP plan_close is paused retains old pins");
}
static void threaded_round(void){
 for(unsigned mode=0;mode<3;mode++){
  struct replacement_fixture *f=make(true);pthread_t threads[2];struct thread_arg args[2];
  if(mode==1)f->f.owners[1].fail_kind=4,f->f.owners[1].fail_n=2;
  if(mode==2)f->f.owners[1].bad_kind=3,f->f.owners[1].bad_n=11; /* final validate VMCS02 */
  for(unsigned c=0;c<2;c++){assert(nsrr_before_entry(&f->round,c));args[c]=(struct thread_arg){f,c,false,false};assert(!pthread_create(&threads[c],0,run_thread,&args[c]));}
  for(unsigned c=0;c<2;c++)assert(!pthread_join(threads[c],0));
  if(!mode){assert(args[0].result&&args[1].result&&!f->faulted);
   for(unsigned c=0;c<2;c++){assert(f->round.committed[c]&&f->round.refs[c].phase==NOSR_COMMITTED&&nsrr_before_entry(&f->round,c));assert(f->f.owners[c].current==f->f.owners[c].vcpu.vmcs01);}
  }else{assert(!args[0].result&&!args[1].result&&f->faulted&&f->round.replacement.state==TSR_POISONED);for(unsigned c=0;c<2;c++)assert(!nsrr_before_entry(&f->round,c));}
  free(f);
 }
 puts("PASS N47B real two-owner round, final exact release, AP restore and validation retain-stop");
}
int main(void){successful();primitive_failures();final_failures();corruptions();owner_guards();root_fields();threaded_round();first_command_abort();return 0;}
