#include "n47_fixture.inc"
#include "typed_root_source_replace.h"
#include "nested_source_replace_memory.h"
#include <sched.h>
static atomic_uint pause_kind,paused,proceed,close_paused,close_continue;
static pthread_mutex_t domain_lock=PTHREAD_MUTEX_INITIALIZER;
static struct fixture *current_f;
bool __real_nrc_end(struct nrc_session *,uint64_t);
bool __real_nrc_guard(struct nrc_session *,uint64_t);
bool __real_typed_root_registry_begin_guarded(struct typed_root_registry *,enum nested_context_operation,bool (*)(void *),void *);
bool __wrap_typed_root_registry_begin_guarded(struct typed_root_registry *b,enum nested_context_operation k,bool (*fn)(void *),void *ctx){
 bool ok=__real_typed_root_registry_begin_guarded(b,k,fn,ctx);
 if(ok&&tls_cpu==0&&atomic_load(&pause_kind)==7){atomic_store(&paused,1);while(!atomic_load_explicit(&proceed,memory_order_relaxed))sched_yield();}return ok;
}
static void pause_at(unsigned kind){if(tls_cpu==0&&atomic_load(&pause_kind)==kind){atomic_store(&paused,1);while(!atomic_load(&proceed))sched_yield();}}
static void pause_authority(void){pause_at(1);}
static void pause_provider(void){pause_at(2);}
static void pause_allocator(void){pause_at(3);}
bool __wrap_nrc_end(struct nrc_session *s,uint64_t nonce){if(current_f&&s==&current_f->owners[0].atomic.fault.permit.source)pause_at(5);return __real_nrc_end(s,nonce);}
bool __wrap_nrc_guard(struct nrc_session *s,uint64_t nonce){if(current_f&&tls_cpu==0&&current_f->owners[0].atomic.entry_active)pause_at(6);return __real_nrc_guard(s,nonce);}
struct replacement_fixture {
 struct fixture f;struct tr_source_replace sr;struct tsr_ticket ticket;
 struct nested_root_catalog next;struct nrc_span spans[12];struct nrc_root roots[3];struct ad_updater_gate gate;
 struct nsrm_plan plan;struct nested_domain domain;
 uint64_t reference[2];unsigned fail_park,fail_restore,corrupt_receipt,completed,late_bank;
 bool fail_change,fail_complete,abort_complete,reenter,pause_close;
};
bool __real_ad_updater_gate_idle(struct ad_updater_gate *,struct ad_entry_stamp);
bool __wrap_ad_updater_gate_idle(struct ad_updater_gate *g,struct ad_entry_stamp stamp){
 bool ok=__real_ad_updater_gate_idle(g,stamp);struct replacement_fixture *f=(void *)current_f;
 if(ok&&f&&g==&f->gate&&f->completed)pause_at(4);
 return ok;
}
static bool splan(void *p,struct tsr_ticket t,struct nested_domain_maintenance_ticket *out){
 struct replacement_fixture *f=p;assert(!tls_depth&&t.nonce);assert(!pthread_mutex_lock(&domain_lock));
 bool ok=f->domain.gate==NESTED_DOMAIN_OPEN&&f->domain.maintenance_epoch!=UINT64_MAX;
 if(ok)*out=(struct nested_domain_maintenance_ticket){f->domain.id,f->domain.incarnation,f->domain.maintenance_epoch+1,NESTED_DOMAIN_SOURCE_REPLACE};
 assert(!pthread_mutex_unlock(&domain_lock));return ok;
}
static bool sclose(void *p,struct tsr_ticket t){
 struct replacement_fixture *f=p;assert(!tls_depth&&t.nonce);struct nested_domain_maintenance_ticket out;
 assert(!pthread_mutex_lock(&domain_lock));bool ok=nested_domain_maintenance_begin(&f->domain,NESTED_DOMAIN_SOURCE_REPLACE,&out)&&out.domain_id==t.domain.domain_id&&out.incarnation==t.domain.incarnation&&out.epoch==t.domain.epoch&&out.purpose==t.domain.purpose;assert(!pthread_mutex_unlock(&domain_lock));
 if(f->pause_close){atomic_store(&close_paused,1);while(!atomic_load(&close_continue))sched_yield();}return ok;
}
static bool strict_stopped(void *p){struct owner *o=p;struct replacement_fixture *f=(void *)o->f;assert(!tls_depth);assert(!pthread_mutex_lock(&domain_lock));bool ok=o->stopped&&f->domain.gate==NESTED_DOMAIN_OPEN;assert(!pthread_mutex_unlock(&domain_lock));return ok;}
static bool sstopped(void *p,unsigned c,struct tsr_ticket t){struct replacement_fixture *f=p;assert(!tls_depth);assert(!pthread_mutex_lock(&domain_lock));bool ok=c<2&&f->f.owners[c].stopped&&f->domain.gate==NESTED_DOMAIN_MAINTENANCE&&t.domain.domain_id==f->domain.id&&t.domain.incarnation==f->domain.incarnation&&t.domain.epoch==f->domain.maintenance_epoch&&t.domain.purpose==NESTED_DOMAIN_SOURCE_REPLACE&&!f->domain.cpus[c].guest_running;assert(!pthread_mutex_unlock(&domain_lock));return ok;}
static bool spark(void *p,unsigned c,struct tsr_ticket t,const struct tsr_root *roots,size_t count,struct tsr_receipt *out){
 struct replacement_fixture *f=p;assert(sstopped(p,c,t)&&tls_cpu==c&&count==2);struct owner *o=&f->f.owners[c];const struct tsr_root *r=NULL;
 for(size_t n=0;n<count;n++)if(roots[n].key==o->registry.active->key)r=&roots[n];
 assert(r&&r->pins==1&&f->reference[c]==r->eptp);
 assert(!nested_ept_contexts_unpin(&o->registry,o->registry.active));
 assert(!nested_ept_contexts_destroy(&o->registry));
 if(f->fail_park==c+1)return false;
 f->reference[c]=0xdead005e;*out=(struct tsr_receipt){t,*r,c,true};if(f->corrupt_receipt==1)out->root.identity.allocator_lifetime++;return true;
}
static bool srestore(void *p,unsigned c,struct tsr_ticket t,const struct tsr_root *r,struct tsr_receipt *out){
 struct replacement_fixture *f=p;assert(sstopped(p,c,t)&&tls_cpu==c&&r->pins==1);struct owner *o=&f->f.owners[c];
 assert(o->registry.active==r->slot->directory.item&&f->reference[c]==0xdead005e);
 assert(nested_ept_contexts_select(&o->registry,r->key)==NESTED_CONTEXT_INVALID&&!nested_ept_contexts_pin_active(&o->registry));
 if(f->fail_restore==c+1)return false;
 f->reference[c]=r->eptp;*out=(struct tsr_receipt){t,*r,c,true};if(f->corrupt_receipt==2)out->owner^=1;return true;
}
static bool scomplete(void *p,struct tsr_ticket t){struct replacement_fixture *f=p;assert(!tls_depth&&f->sr.state==TSR_COMMITTING);
 if(f->abort_complete)assert(tsr_abort(&f->sr,t));
 if(f->fail_complete)return false;
 for(unsigned c=0;c<2;c++)if(f->reference[c]!=f->sr.fresh[c].eptp)return false;
 assert(!pthread_mutex_lock(&domain_lock));
 for(unsigned c=0;c<2;c++)assert(nested_domain_maintenance_ack(&f->domain,c,t.domain));
 assert(nested_domain_maintenance_publish(&f->domain,t.domain));assert(nested_domain_maintenance_complete(&f->domain,t.domain));f->completed++;assert(!pthread_mutex_unlock(&domain_lock));
 if(f->late_bank==1)assert(ad_updater_gate_close(&f->gate,f->gate.stamp));
 if(f->late_bank==2)assert(nrc_close(&f->next,f->next.identity));
 return true;
}
static bool manage(void *p,uint64_t ticket,const struct nrc_span *s,uint64_t gpa,struct nsrm_authority_page *a){struct replacement_fixture *f=p;assert(!tls_depth&&ticket==f->ticket.nonce&&!f->f.catalog.references&&f->f.catalog.closed&&!f->f.gate.references&&f->f.gate.closed);
 *a=(struct nsrm_authority_page){s->identity,ticket,gpa,s->object,s->lifetime,s->hpa+(gpa-s->gpa),s->table_view?(uint64_t *)s->table_view:NULL,s->permissions,6,true,true,true,true,true};return true;
}
static bool schange(void *p,struct tsr_ticket t,struct nested_root_catalog *old,struct ad_updater_gate *gate,struct nested_root_catalog **out,struct ad_updater_gate **gout,void *contexts[2]){
 struct replacement_fixture *f=p;assert(old==&f->f.catalog&&gate==&f->f.gate&&t.nonce==f->ticket.nonce);
 if(f->fail_change)return false;
 struct nsrm_request req={.ticket=t.nonce,.full_eptp=f->f.roots[0].full_eptp,.l2_gpa=0x7000,.table_gpa=gpas[4],.table_hpa=f->f.spans[4].hpa,.table_object=5,.table_lifetime=54,.expected_old=__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE),.new_leaf=0x210073,.new_gpa=0x210000,.new_hpa=DATA+0x8000,.new_object=22,.new_lifetime=82,.index=7};
 assert(nsrm_prepare(&f->plan,old,manage,f,req)&&nsrm_apply(&f->plan));
 struct nrc_identity id={1,2,4,5};struct ad_entry_stamp stamp={1,2,4,6,5};
 assert(nrc_init_atomic_wb_successor(&f->next,old,old->identity,id,48,f->spans,12,f->roots,3,authority,64));
 assert(ad_updater_gate_init_successor(&f->gate,gate,t.stamp,stamp,64));
 for(size_t n=0;n<old->span_count;n++){struct nrc_span s=old->spans[n];s.identity=id;assert(nrc_add_atomic_span(&f->next,s,(uint64_t *)s.table_view));}
 for(size_t n=0;n<old->root_count;n++){struct nrc_root r=old->roots[n];r.identity=id;r.generation++;assert(nrc_add_root(&f->next,r));}
 assert(nrc_publish(&f->next));*out=&f->next;*gout=&f->gate;contexts[0]=&f->f.owners[0];contexts[1]=&f->f.owners[1];return true;
}
static struct replacement_fixture *ssetup(bool warm){
 struct replacement_fixture *f=calloc(1,sizeof *f);assert(f);setup(&f->f);current_f=&f->f;
 assert(nested_domain_init(&f->domain,1,2,3));
 for(unsigned c=0;c<2;c++){struct nested_domain_token token;assert(nested_domain_vmxon(&f->domain,c,0x300000+c*4096));assert(nested_domain_alloc(&f->domain,0x400000+c*4096,&token));assert(nested_domain_load(&f->domain,c,token));struct nested_domain_cpu_token ct={1,2,c};assert(nested_domain_guest_enter(&f->domain,ct)&&nested_domain_guest_leave(&f->domain,ct));}
 for(unsigned c=0;c<2;c++)f->f.owners[c].atomic.config.consumer_stopped=strict_stopped;
 struct tsr_hooks hooks={splan,sclose,sstopped,spark,srestore,scomplete};
 assert(tsr_init(&f->sr,&f->f.owners[0].atomic,&f->f.owners[1].atomic,&f->f.owners[0].provider,&f->f.owners[1].provider,NULL,NULL,&hooks,f));
 for(unsigned c=0;c<2;c++){struct owner *o=&f->f.owners[c];select_root(o,c);if(warm)read_write(o,true);select_root(o,2);if(warm)read_write(o,true);select_root(o,c);assert(nested_ept_contexts_pin_active(&o->registry));f->reference[c]=o->registry.active->tree.eptp;assert(tr_atomic_entry(&o->atomic,o->registry.active,f->reference[c]));}return f;
}
static void sclosed(struct replacement_fixture *f){assert(tsr_close(&f->sr,&f->ticket));assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_READY);assert(f->f.catalog.closed&&f->f.catalog.references==4&&!f->f.directory.closed&&!f->f.lease.poisoned);}
static void sretired(struct replacement_fixture *f){sclosed(f);for(unsigned c=0;c<2;c++){tls_cpu=c;assert(tsr_retire_owner(&f->sr,c,f->ticket));}assert(f->sr.state==TSR_RETIRED&&!f->f.catalog.references&&!f->f.directory.records);}
static void spublished(struct replacement_fixture *f){sretired(f);assert(tsr_replace(&f->sr,f->ticket,schange,f));assert(f->sr.state==TSR_PUBLISHED);}
static void sprepared(struct replacement_fixture *f){spublished(f);for(unsigned c=0;c<2;c++){tls_cpu=c;assert(tsr_prepare_owner(&f->sr,c,f->ticket));if(c==0)assert(!tsr_finish(&f->sr,f->ticket)&&f->sr.state==TSR_PUBLISHED);}}
static void success(void){struct replacement_fixture *f=ssetup(true);uint64_t oldhpa[2],oldlife[2],oldtick[2];struct nac_trace oldtrace[2];
 for(unsigned c=0;c<2;c++){oldhpa[c]=f->reference[c];oldlife[c]=active(&f->f.owners[c])->directory.identity.allocator_lifetime;oldtick[c]=f->f.owners[c].registry.tick;oldtrace[c]=f->f.owners[c].traces[0].trace;}
 sprepared(f);assert(tsr_finish(&f->sr,f->ticket)&&tsr_released(&f->sr,f->ticket)&&!tsr_abort(&f->sr,f->ticket));
 assert(f->f.catalog.superseded&&f->f.gate.superseded&&f->f.lease.type_epoch==1&&f->completed==1);
 assert(!ad_updater_gate_reopen(&f->f.gate,f->f.stamp,true,&f->f.stamp));
 for(unsigned c=0;c<2;c++){struct owner *o=&f->f.owners[c];tls_cpu=c;assert(o->invepts==2);assert(o->reserved==3&&o->registry.tick>oldtick[c]&&active(o)->directory.identity.allocator_lifetime>oldlife[c]);
  assert(o->registry.active->tree.eptp==oldhpa[c]); /* deliberately real pool HPA reuse */
  for(unsigned n=0;n<2;n++){lock(o);assert(tr_local_completed_matches(&o->provider,&o->slots[n]));unlock(o);assert(o->bindings[n].released&&!o->traces[n].usable&&!o->traces[n].registered);}
  assert(tr_atomic_entry(&o->atomic,o->registry.active,f->reference[c]));struct nested_ept_result out;assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_FILLED&&out.hpa==DATA+0x8000);
  assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,2,&out)==TR_ATOMIC_PROMOTED);
  assert(!nac_trace_guard(&o->atomic.fault.permit,o->atomic.fault.ticket,0x7000,&oldtrace[c]));
 }
 assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&~UINT64_C(0x300))==0x210073);assert(__atomic_load_n(&tables[8][7],__ATOMIC_ACQUIRE)==0x208373);
 free(f);puts("PASS N47A production successful same-HPA replacement and stable receipts");}
static void negative(void){for(unsigned mode=0;mode<10;mode++){struct replacement_fixture *f=ssetup(true);unsigned frees=0;
 if(mode<4){sclosed(f);f->fail_park=mode==0?1:0;f->corrupt_receipt=mode==1?1:0;f->f.owners[0].injection=mode==2?PROVIDER_FAIL:mode==3?PIN_CLEANUP:NONE;tls_cpu=0;frees=f->f.owners[0].frees;assert(!tsr_retire_owner(&f->sr,0,f->ticket));assert(f->f.owners[0].pool.live);if(mode<2)assert(f->f.owners[0].registry.active->pins==1);if(mode==3){assert(f->f.owners[0].slots[1].directory.receipt.live);assert(f->f.owners[0].frees==frees);}}
 else if(mode==4){sretired(f);f->fail_change=true;assert(!tsr_replace(&f->sr,f->ticket,schange,f));}
 else if(mode<7){spublished(f);tls_cpu=0;assert(tsr_prepare_owner(&f->sr,0,f->ticket));tls_cpu=1;f->fail_restore=mode==5?2:0;f->corrupt_receipt=mode==6?2:0;assert(!tsr_prepare_owner(&f->sr,1,f->ticket));assert(f->f.owners[1].registry.active->pins==1);}
 else{sprepared(f);f->fail_complete=mode==7;f->abort_complete=mode==8;if(mode==9)f->reference[1]^=4096;assert(!tsr_finish(&f->sr,f->ticket));}
 assert(f->sr.state==TSR_POISONED&&!tsr_released(&f->sr,f->ticket));tls_cpu=0;assert(!tr_atomic_ready(&f->f.owners[0].atomic));free(f);}
 puts("PASS N47A ten retained-stop failure paths");}
static void wrong_calls(void){struct replacement_fixture *f=ssetup(true);sclosed(f);struct tsr_ticket bad=f->ticket;bad.nonce++;tls_cpu=0;
 assert(!tsr_retire_owner(&f->sr,0,bad)&&!tsr_retire_owner(&f->sr,1,f->ticket)&&!tsr_prepare_owner(&f->sr,0,f->ticket)&&!tsr_finish(&f->sr,f->ticket));
 for(unsigned c=0;c<2;c++){struct owner *o=&f->f.owners[c];tls_cpu=c;struct nested_ept_result out;assert(!tr_atomic_entry(&o->atomic,o->registry.active,f->reference[c]));assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_REJECTED);assert(nested_ept_contexts_select(&o->registry,f->f.roots[0].full_eptp)==NESTED_CONTEXT_INVALID);assert(!nested_ept_contexts_pin_active(&o->registry));assert(!nested_ept_contexts_unpin(&o->registry,o->registry.active));assert(nested_ept_contexts_invalidate(&o->registry,2,0)==NESTED_CONTEXT_INVALID);assert(tsr_retire_owner(&f->sr,c,f->ticket));assert(!tsr_retire_owner(&f->sr,c,f->ticket));}
 free(f);puts("PASS N47A wrong stale duplicate owner command and normal admissions");}
static const struct nested_context_guards *original_guards;
static const struct nested_context_transactions *original_tx;
static void relaxed_pause(unsigned kind){if(tls_cpu==0&&atomic_load(&pause_kind)==kind){atomic_store(&paused,1);while(!atomic_load_explicit(&proceed,memory_order_relaxed))sched_yield();}}
static bool proxy_select(void *ctx,struct nested_ept_context_item *i){bool ok=original_guards->select(ctx,i);if(ok)relaxed_pause(8);return ok;}
static bool proxy_reset(void *ctx,struct nested_ept_context_item *i){return original_guards->pre_reset(ctx,i);}
static bool proxy_retire(void *ctx,struct nested_ept_context_item *i){return original_guards->pre_retire(ctx,i);}
static const struct nested_context_guards proxy_guards={proxy_select,proxy_reset,proxy_retire};
static void proxy_retain(void *ctx,void *token,struct nested_ept_context_item *i){original_tx->retain(ctx,token,i);relaxed_pause(9);}
static void proxy_end(void *ctx,enum nested_context_operation kind){relaxed_pause(10);original_tx->end(ctx,kind);}
struct thread_arg{struct replacement_fixture *f;unsigned c,action;};
static atomic_uint finish_result;
static void *worker(void *p){struct thread_arg *a=p;tls_cpu=a->c;struct owner *o=&a->f->f.owners[a->c];
 if(a->action==1){struct nested_ept_result out;assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_FILLED);}
 else if(a->action==2)assert(tsr_retire_owner(&a->f->sr,a->c,a->f->ticket));
 else if(a->action==3)assert(tsr_prepare_owner(&a->f->sr,a->c,a->f->ticket));
 else if(a->action==4)atomic_store(&finish_result,tsr_finish(&a->f->sr,a->f->ticket)?1:2);
 else if(a->action==5)assert(nested_ept_contexts_select(&o->registry,a->f->f.roots[1].full_eptp)==NESTED_CONTEXT_OK);
 else if(a->action==9)assert(nested_ept_contexts_select(&o->registry,o->registry.active->key)==NESTED_CONTEXT_OK);
 else if(a->action==8)assert(nested_ept_contexts_select(&o->registry,a->f->f.roots[1].full_eptp)!=NESTED_CONTEXT_OK);
 else if(a->action==7)assert(tsr_close(&a->f->sr,&a->f->ticket));
 else if(a->action==6)assert(!tr_atomic_entry(&o->atomic,o->registry.active,o->registry.active->tree.eptp)&&!o->atomic.poisoned);
 return NULL;}
static void threaded(void){struct replacement_fixture *f=ssetup(false);pthread_t t;struct thread_arg a={f,0,1};atomic_store(&pause_kind,1);atomic_store(&paused,0);atomic_store(&proceed,0);assert(!pthread_create(&t,NULL,worker,&a));while(!atomic_load(&paused))sched_yield();
 assert(tsr_close(&f->sr,&f->ticket));assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_WAIT);assert(!f->f.catalog.closed&&!f->f.gate.closed);atomic_store(&proceed,1);assert(!pthread_join(t,NULL));atomic_store(&pause_kind,0);assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_READY);
 pthread_t threads[2];struct thread_arg args[2]={{f,0,2},{f,1,2}};for(unsigned c=0;c<2;c++)assert(!pthread_create(&threads[c],NULL,worker,&args[c]));for(unsigned c=0;c<2;c++)assert(!pthread_join(threads[c],NULL));
 assert(tsr_replace(&f->sr,f->ticket,schange,f));for(unsigned c=0;c<2;c++){args[c].action=3;assert(!pthread_create(&threads[c],NULL,worker,&args[c]));}for(unsigned c=0;c<2;c++)assert(!pthread_join(threads[c],NULL));assert(tsr_finish(&f->sr,f->ticket));free(f);puts("PASS N47A genuine two-owner threads and admitted fault drain");}
static void stale_actual_permit(void){
 struct replacement_fixture *f=ssetup(true);struct owner *o=&f->f.owners[0];struct nac_ticket old=o->atomic.fault.ticket;struct nac_permit *saved=&o->atomic.fault.permit;struct nac_trace trace=o->traces[0].trace;
 struct trl_token typed;lock(o);assert(trl_token_read(&f->f.lease,&o->slots[0].typed,&typed));unlock(o);
 sprepared(f);assert(tsr_finish(&f->sr,f->ticket));pthread_t thread;struct thread_arg arg={f,0,1};atomic_store(&pause_kind,1);atomic_store(&paused,0);atomic_store(&proceed,0);assert(!pthread_create(&thread,NULL,worker,&arg));while(!atomic_load(&paused))sched_yield();tls_cpu=0;
 assert(saved==&o->atomic.fault.permit&&saved->active&&saved->source.nonce>old.source_nonce&&saved->updater.nonce>old.updater_nonce);
 uint64_t refs=f->next.references,grefs=f->gate.references;
 assert(!nac_guard(saved,old)&&!nac_end(saved,old));assert(!nrc_guard(&saved->source,old.source_nonce)&&!nrc_end(&saved->source,old.source_nonce));
 assert(!ad_updater_gate_guard(&f->gate,&saved->updater,f->gate.stamp,old.updater_nonce)&&!ad_updater_gate_release(&f->gate,&saved->updater,f->gate.stamp,old.updater_nonce));
 assert(f->next.references==refs&&f->gate.references==grefs&&nac_guard(saved,o->atomic.fault.ticket));
 assert(!nac_trace_guard(saved,o->atomic.fault.ticket,0x7000,&trace));
 lock(o);assert(!trl_admit(&f->f.lease,&o->atomic.fault.operation,o->atomic.fault.nonce,&active(o)->typed,typed,TRL_WB,true,true));unlock(o);
 atomic_store(&proceed,1);assert(!pthread_join(thread,NULL));atomic_store(&pause_kind,0);assert(!saved->active&&!f->gate.references);free(f);puts("PASS N47A actual fault.permit same-storage old NAC NRC updater typed trace rejection");
}
static void concurrent_drain_stages(void){
 for(unsigned kind=2;kind<=6;kind++){if(kind==4)continue;
  struct replacement_fixture *f=ssetup(kind==2||kind==3||kind==6);pthread_t thread;struct thread_arg arg={f,0,kind==2||kind==3?5:kind==6?6:1};atomic_store(&pause_kind,kind);atomic_store(&paused,0);atomic_store(&proceed,0);assert(!pthread_create(&thread,NULL,worker,&arg));while(!atomic_load(&paused))sched_yield();
  assert(tsr_close(&f->sr,&f->ticket));assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_WAIT);assert(!f->f.catalog.closed&&!f->f.gate.closed);
  atomic_store(&proceed,1);assert(!pthread_join(thread,NULL));atomic_store(&pause_kind,0);assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_READY);assert(!f->f.owners[0].atomic.poisoned&&tsr_abort(&f->sr,f->ticket));free(f);
 }
 puts("PASS N47A provider allocator cleanup entry admitted drain threads");
}
static void final_abort_race(void){struct replacement_fixture *f=ssetup(true);sprepared(f);pthread_t thread;struct thread_arg arg={f,0,4};atomic_store(&pause_kind,4);atomic_store(&paused,0);atomic_store(&proceed,0);atomic_store(&finish_result,0);assert(!pthread_create(&thread,NULL,worker,&arg));while(!atomic_load(&paused))sched_yield();
 assert(f->domain.gate==NESTED_DOMAIN_OPEN&&!tsr_released(&f->sr,f->ticket));assert(tsr_abort(&f->sr,f->ticket));atomic_store(&proceed,1);assert(!pthread_join(thread,NULL));atomic_store(&pause_kind,0);assert(atomic_load(&finish_result)==2&&f->sr.state==TSR_POISONED&&!tsr_released(&f->sr,f->ticket));free(f);puts("PASS N47A real-thread domain-complete versus abort cannot publish release");}
static void close_handoff(void){
 struct replacement_fixture *f=ssetup(false);pthread_t fault_thread,close_thread;struct thread_arg fault_arg={f,0,1},close_arg={f,1,7};f->pause_close=true;
 atomic_store(&pause_kind,1);atomic_store(&paused,0);atomic_store(&proceed,0);atomic_store(&close_paused,0);atomic_store(&close_continue,0);
 assert(!pthread_create(&fault_thread,NULL,worker,&fault_arg));while(!atomic_load(&paused))sched_yield();assert(!pthread_create(&close_thread,NULL,worker,&close_arg));while(!atomic_load(&close_paused))sched_yield();
 assert(f->domain.gate==NESTED_DOMAIN_MAINTENANCE&&f->sr.state==TSR_BOUND&&f->ticket.domain.purpose==NESTED_DOMAIN_SOURCE_REPLACE);
 atomic_store(&proceed,1);assert(!pthread_join(fault_thread,NULL));assert(!f->f.owners[0].atomic.poisoned&&!f->f.owners[0].atomic.fault.active);atomic_store(&close_continue,1);assert(!pthread_join(close_thread,NULL));atomic_store(&pause_kind,0);
 assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_READY);free(f);puts("PASS N47A exact prebound ticket routes fault while physical close hook is paused");
}
static void abort_registry_thread(void){struct replacement_fixture *f=ssetup(true);pthread_t thread;struct thread_arg arg={f,0,8};atomic_store(&pause_kind,7);atomic_store(&paused,0);atomic_store(&proceed,0);assert(!pthread_create(&thread,NULL,worker,&arg));while(!atomic_load(&paused))sched_yield();
 assert(tsr_close(&f->sr,&f->ticket)&&tsr_abort(&f->sr,f->ticket));atomic_store_explicit(&proceed,1,memory_order_relaxed);assert(!pthread_join(thread,NULL));atomic_store(&pause_kind,0);assert(f->sr.state==TSR_POISONED&&!tsr_released(&f->sr,f->ticket)&&f->f.owners[0].pool.live);free(f);puts("PASS N47A remote abort versus owner registry path without synthetic ordering");}
static void final_bank_guards(void){for(unsigned mode=0;mode<7;mode++){struct replacement_fixture *f=ssetup(true);sprepared(f);switch(mode){
 case 0:assert(ad_updater_gate_close(&f->gate,f->gate.stamp));break;
 case 1:ad_updater_gate_poison(&f->gate);break;
 case 2:{struct ad_entry_stamp next;assert(ad_updater_gate_close(&f->gate,f->gate.stamp)&&ad_updater_gate_reopen(&f->gate,f->gate.stamp,true,&next));break;}
 case 3:assert(nrc_close(&f->next,f->next.identity));break;
 case 4:__atomic_store_n(&f->next.fault,1,__ATOMIC_RELEASE);break;
 case 5:f->late_bank=1;break;case 6:f->late_bank=2;break;}
 assert(!tsr_finish(&f->sr,f->ticket)&&f->sr.state==TSR_POISONED&&!tsr_released(&f->sr,f->ticket));for(unsigned c=0;c<2;c++)assert(f->f.owners[c].registry.active->pins==1);free(f);
 }puts("PASS N47A final new-bank close poison stamp and completion-time fault guards");}
static void owner_local_windows(void){
 for(unsigned phase=8;phase<=10;phase++){struct replacement_fixture *f=ssetup(true);struct owner *o=&f->f.owners[0];struct nested_context_transactions tx;
  if(phase==9){sprepared(f);assert(tsr_finish(&f->sr,f->ticket));o->injection=FLUSH_FAIL;original_tx=o->registry.transactions;tx=*original_tx;tx.retain=proxy_retain;o->registry.transactions=&tx;}
  else if(phase==8){original_guards=o->registry.guards;o->registry.guards=&proxy_guards;}
  else{original_tx=o->registry.transactions;tx=*original_tx;tx.end=proxy_end;o->registry.transactions=&tx;}
  pthread_t thread;struct thread_arg arg={f,0,phase==9?8:9};atomic_store(&pause_kind,phase);atomic_store(&paused,0);atomic_store(&proceed,0);assert(!pthread_create(&thread,NULL,worker,&arg));while(!atomic_load(&paused))sched_yield();
  if(phase==9)assert(tsr_released(&f->sr,f->ticket));
  else{assert(tsr_close(&f->sr,&f->ticket));assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_WAIT);}
  atomic_store_explicit(&proceed,1,memory_order_relaxed);assert(!pthread_join(thread,NULL));atomic_store(&pause_kind,0);
  if(phase==9)assert(!tsr_released(&f->sr,f->ticket)&&o->atomic.poisoned);
  else assert(tsr_drain(&f->sr,f->ticket)==TSR_DRAIN_READY);
  free(f);
 }
 puts("PASS N47A warm SELECT late tick pending end and post-release owner-local failure windows");
}
static void preflight(void){for(unsigned mode=0;mode<6;mode++){struct replacement_fixture *f=ssetup(true);switch(mode){case 0:f->f.directory.next_id=UINT64_MAX;break;case 1:f->f.lease.budget=f->f.lease.reserved;break;case 2:f->f.owners[0].registry.tick=UINT64_MAX;break;case 3:f->f.serial=UINT64_MAX;break;case 4:f->sr.next_command=UINT64_MAX;break;case 5:f->f.roots[0].generation=UINT64_MAX;break;}assert(!tsr_close(&f->sr,&f->ticket)&&f->sr.state==TSR_OPEN&&f->domain.gate==NESTED_DOMAIN_OPEN);free(f);}puts("PASS N47A bounded preflight counter and capacity rejection");}
int main(void){setbuf(stdout,NULL);preflight();success();negative();wrong_calls();threaded();close_handoff();abort_registry_thread();owner_local_windows();stale_actual_permit();concurrent_drain_stages();final_abort_race();final_bank_guards();return 0;}
