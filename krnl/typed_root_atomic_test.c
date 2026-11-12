/* Actual contexts/private pools/typed bridge/local provider. Physical CPU and
 * INVEPT are test hooks; this is not VMX or physical invalidation evidence. */
#include "typed_root_atomic.h"
#include "typed_root_local_provider.h"
#include "nested_owner_pool.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PAGES 96u
#define SLOTS 16u
#define TRACES 32u
#define DATA UINT64_C(0x500000)
static _Alignas(4096) unsigned char memory[2][PAGES][4096];
static _Alignas(4096) uint64_t tables[9][512];
static const uint64_t gpas[9]={0x11000,0x27000,0x38000,0x50000,0x71000,0xc0000,0xc7000,0xdd000,0xf1000};
enum injection { NONE,ALLOC_FAIL,FLUSH_FAIL,FINAL_GUARD,AUTHORITY_FAIL,REGISTER_FAIL,
 UPDATER_CLEANUP,SOURCE_CLEANUP,PROVIDER_FAIL,COMMIT_FAIL,PIN_CLEANUP,RESERVE_FAIL,END_CLEANUP };
struct fixture;
struct owner {
 struct fixture *f;unsigned cpu,reserved,allocations,frees,flushes,authorities,invepts;
 enum injection injection;bool stopped;unsigned fail_allocation;
 struct tr_atomic_owner atomic;
 struct typed_root_registry bridge;
 struct nested_ept_contexts registry;
 struct nested_owner_pool pool;
 struct nested_sparse_page pages[PAGES];struct nested_owner_slot pool_slots[PAGES];
 struct tr_registry_slot slots[SLOTS];struct tr_atomic_binding bindings[SLOTS];
 struct tr_atomic_trace traces[TRACES];
 struct tr_local_provider provider;struct tr_local_completed completed[SLOTS];
};
struct fixture {
 struct nested_root_catalog catalog;struct nrc_span spans[11];struct nrc_root roots[3];
 struct nrc_identity id;struct ad_updater_gate gate;struct ad_entry_stamp stamp;
 struct typed_directory directory;struct typed_root_lease lease;
 struct owner owners[2];unsigned depth,cpu;uint64_t serial;
};
static void lock(void *p){struct owner *o=p;assert(!o->f->depth);o->f->depth=1;}
static void unlock(void *p){struct owner *o=p;assert(o->f->depth==1);o->f->depth=0;}
static bool provider_lock(void *p){lock(p);return true;}
static unsigned current_cpu(void *p){struct owner *o=p;return o->f->cpu;}
static bool stopped(void *p){struct owner *o=p;assert(!o->f->depth);return o->stopped;}
static bool provider_healthy(void *p){struct owner *o=p;assert(o->f->depth);return o->stopped;}
static bool authority(void *p,const struct nrc_span *s,uint64_t gpa,struct nrc_atomic_page *out){
 struct owner *o=p;assert(!o->f->depth);o->authorities++;
 assert(o->atomic.fault.active&&o->atomic.fault.transient&&o->atomic.fault.permit.active);
 if(o->injection==AUTHORITY_FAIL)return false;
 uint64_t off=gpa-s->gpa;
 *out=(struct nrc_atomic_page){.backing={s->object,s->lifetime,s->hpa+off,
  s->table_view?(const uint64_t *)((uintptr_t)s->table_view+off):NULL,s->permissions,6,true},
  .atomic_view=s->table_view?(uint64_t *)((uintptr_t)s->table_view+off):NULL,.pinned_writable=s->role==NRC_TABLE};
 if(o->injection==REGISTER_FAIL)o->f->directory.version=UINT64_MAX;
 return true;
}
static bool alloc(void *p,struct nested_sparse_page *out){
 struct owner *o=p;assert(!o->f->depth);o->allocations++;
 if(o->injection==ALLOC_FAIL||o->allocations==o->fail_allocation)return false;
 assert(o->bridge.directory.busy||o->bridge.refill_operation);
 return nested_owner_pool_alloc(&o->pool,out);
}
static void release(void *p,struct nested_sparse_page page){
 struct owner *o=p;assert(!o->f->depth);
 for(unsigned i=0;i<o->reserved;i++)if(o->slots[i].directory.record.hpa==page.hpa){
  lock(o);assert(tr_local_completed_matches(&o->provider,&o->slots[i]));unlock(o);
  assert(o->bindings[i].released&&!o->bindings[i].pin.live);
 }
 o->frees++;nested_owner_pool_free(&o->pool,page);assert(!o->pool.fault);
}
static bool flush(void *p,uint64_t eptp){
 struct owner *o=p;assert(!o->f->depth&&(eptp&4095)==0x5e);o->flushes++;
 if(o->atomic.fault.active){
  assert(o->atomic.fault.transient&&o->atomic.fault.permit.active&&
   o->atomic.fault.permit.source.live&&o->atomic.fault.permit.updater.live);
  if(o->injection==FINAL_GUARD)o->stopped=false;
  if(o->injection==UPDATER_CLEANUP)__atomic_store_n(&o->f->gate.lock,1,__ATOMIC_RELEASE);
  if(o->injection==SOURCE_CLEANUP)__atomic_store_n(&o->f->catalog.lock,1,__ATOMIC_RELEASE);
  if(o->injection==END_CLEANUP){
   struct td_record *r=&o->atomic.fault.binding->slot->directory.record;
   lock(o);r->state=TD_CANDIDATE;r->constructing_nonce=o->atomic.fault.nonce;unlock(o);
  }
 }
 return o->injection!=FLUSH_FAIL;
}
static bool reserve(void *p,struct tr_registry_slot **out,uint64_t *lifetime){
 struct owner *o=p;assert(o->f->depth);
 if(o->reserved==SLOTS||o->injection==RESERVE_FAIL)return false;
 *out=&o->slots[o->reserved++];*lifetime=100+o->cpu*SLOTS+o->reserved;return true;
}
static bool invalidate(void *p,uint64_t eptp,struct tr_local_invalidation *out){
 struct owner *o=p;assert(!o->f->depth&&(eptp&4095)==0x5e);o->invepts++;
 assert(!o->atomic.fault.active);
 *out=(struct tr_local_invalidation){.full_eptp=eptp,.cpu=o->cpu,.acked=UINT64_C(1)<<o->cpu,.success=true};
 return o->injection!=PROVIDER_FAIL;
}
static bool evidence(void *p,struct tr_registry_slot *s,struct tr_registry_retire_scope scope,
 struct tr_registry_root_evidence *out){
 struct owner *o=p;assert(!o->f->depth);
 bool ok=tr_local_provider_flush(&o->provider,s,scope,out);
 if(ok&&o->injection==COMMIT_FAIL)o->f->directory.version=UINT64_MAX;
 if(ok&&o->injection==PIN_CLEANUP)__atomic_store_n(&o->f->catalog.lock,1,__ATOMIC_RELEASE);
 return ok;
}
static void setup_with_trace_count(struct fixture *f,size_t trace_count){
 memset(f,0,sizeof *f);memset(tables,0,sizeof tables);
 f->id=(struct nrc_identity){1,2,3,4};f->stamp=(struct ad_entry_stamp){1,2,3,5,4};
 assert(nrc_init_atomic_wb(&f->catalog,f->id,48,f->spans,11,f->roots,3,authority));
 for(unsigned i=0;i<9;i++){
  struct nrc_span s={.identity=f->id,.object=i+1,.lifetime=50+i,.gpa=gpas[i],.hpa=0x100000+i*4096,
   .bytes=4096,.table_view=tables[i],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,tables[i]));
 }
 for(unsigned i=0;i<2;i++){
  struct nrc_span s={.identity=f->id,.object=20+i,.lifetime=80+i,.gpa=0x200000+i*0x8000,
   .hpa=DATA+i*0x4000,.bytes=4096,.permissions=3,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
  assert(nrc_add_atomic_span(&f->catalog,s,NULL));
 }
 const unsigned indices[3]={0,1,5};
 for(unsigned i=0;i<3;i++){
  uint64_t root=gpas[indices[i]];
  assert(nrc_add_root(&f->catalog,(struct nrc_root){f->id,30+i,i,10+i,90+i,root|0x5e,root}));
 }
 __atomic_store_n(&tables[0][0],gpas[2]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[1][0],gpas[2]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[2][0],gpas[3]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[3][0],gpas[4]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[4][7],0x200073,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[5][0],gpas[6]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[6][0],gpas[7]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[7][0],gpas[8]|7,__ATOMIC_RELAXED);
 __atomic_store_n(&tables[8][7],0x208073,__ATOMIC_RELAXED);
 assert(nrc_publish(&f->catalog));assert(ad_updater_gate_init(&f->gate,f->stamp));
 assert(td_init(&f->directory,f->id.domain,f->id.incarnation,3,1));
 assert(trl_init_atomic_wb(&f->lease,&f->directory,(struct trl_backing){DATA,99,true},2*SLOTS));
 for(unsigned cpu=0;cpu<2;cpu++){
  struct owner *o=&f->owners[cpu];o->f=f;o->cpu=cpu;o->stopped=true;
  for(unsigned i=0;i<PAGES;i++)o->pages[i]=(struct nested_sparse_page){0x900000+cpu*0x100000+i*4096,memory[cpu][i]};
  assert(nested_owner_pool_init(&o->pool,cpu,100+cpu,o->pages,o->pool_slots,PAGES,48));
  const struct nested_sparse_ops sparse={alloc,release,flush};
  assert(nested_ept_contexts_init_config(&o->registry,&sparse,o,48,2,true,true,NULL,NULL));
  const struct tr_registry_root_provider provider={reserve,evidence};
  const struct tr_atomic_config config={.catalog=&f->catalog,.catalog_identity=f->id,.authority_context=o,
   .gate=&f->gate,.stamp=f->stamp,.slots=o->slots,.bindings=o->bindings,.slot_count=SLOTS,
   .traces=o->traces,.trace_count=trace_count,.lock=lock,.unlock=unlock,
   .current_cpu=current_cpu,.consumer_stopped=stopped,.context=o};
  assert(tr_atomic_init(&o->atomic,&o->bridge,&o->registry,&f->lease,cpu,&provider,o,&config));
  o->provider=(struct tr_local_provider){.bridge=&o->bridge,.slots=o->slots,.completed=o->completed,
   .slot_count=SLOTS,.owner=cpu,.serial=&f->serial,.lock=provider_lock,.unlock=unlock,
   .current_cpu=current_cpu,.healthy=provider_healthy,.invalidate=invalidate,.context=o};
 }
}
static void setup(struct fixture *f){setup_with_trace_count(f,TRACES);}
static struct tr_registry_slot *active(struct owner *o){
 assert(o->registry.active);
 for(unsigned i=0;i<o->reserved;i++)if(&o->slots[i].directory==o->registry.active->transaction_token)return &o->slots[i];
 abort();
}
static void select_root(struct owner *o,unsigned index){
 o->f->cpu=o->cpu;
 assert(nested_ept_contexts_select(&o->registry,o->f->roots[index].full_eptp)==NESTED_CONTEXT_OK);
 assert(o->registry.active->tree.hardware_ad&&(o->registry.active->tree.eptp&4095)==0x5e);
}
static void read_write(struct owner *o,bool write){
 struct nested_ept_result result;struct tr_registry_slot *s=active(o);
 assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&result)==TR_ATOMIC_FILLED);
 assert(result.status==NESTED_EPT_OK&&s->directory.record.refills==1);
 assert(!o->atomic.fault.active&&!o->bridge.refill_operation&&!o->f->directory.operations);
 /* An escaped trace is a real registration, not merely test metadata. */
 struct td_operation op;uint64_t nonce;struct td_retire_ticket ticket;
 lock(o);assert(td_begin(&o->f->directory,&op,&nonce));
 assert(!td_retire_begin(&o->f->directory,&op,nonce,&s->directory.record,s->directory.identity,&ticket));
 assert(td_end(&o->f->directory,&op,nonce));unlock(o);
 assert(typed_root_registry_lookup(&o->bridge,o->registry.active,0x7000,1,&result)==NESTED_SPARSE_BAD_LOOKUP);
 assert(typed_root_registry_fill(&o->bridge,o->registry.active,0x8000,&result)==NESTED_SPARSE_INVALID);
 if(write){
  unsigned old=o->flushes;
  assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,2,&result)==TR_ATOMIC_PROMOTED);
  assert(o->flushes==old+1&&s->directory.record.refills==1);
 }
}
static void basic(void){
 struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);
 const unsigned sequence[]={0,1,2,0};
 for(unsigned round=0;round<4;round++)for(unsigned cpu=0;cpu<2;cpu++){
  struct owner *o=&f->owners[cpu];select_root(o,sequence[round]);read_write(o,true);
  assert(o->registry.count<=2&&o->pool.live<=PAGES);
 }
 assert((__atomic_load_n(&tables[4][7],__ATOMIC_ACQUIRE)&0x300)==0x300);
 assert((__atomic_load_n(&tables[8][7],__ATOMIC_ACQUIRE)&0x300)==0x300);
 for(unsigned cpu=0;cpu<2;cpu++){
  struct owner *o=&f->owners[cpu];f->cpu=cpu;
  assert(o->reserved==4&&o->invepts==2);
  uint64_t epoch=f->lease.type_epoch,ad=f->gate.stamp.epoch,source=f->roots[0].generation;
  struct nested_ept_context_item *item=o->registry.active;uint64_t generation=item->tree.generation;
  assert(nested_ept_contexts_invalidate(&o->registry,1,item->key)==NESTED_CONTEXT_OK);
  assert(item->tree.generation==generation+1&&active(o)->directory.record.refills==0&&
   f->lease.type_epoch==epoch&&f->gate.stamp.epoch==ad&&f->roots[0].generation==source);
  read_write(o,true);assert(nested_ept_contexts_destroy(&o->registry));
  assert(!o->pool.live&&o->allocations==o->frees);
 }
 assert(!f->catalog.references&&!f->directory.records&&!f->directory.operations&&!f->lease.live_tokens);
 assert(nrc_idle(&f->catalog,f->id));free(f);
}
static void fault_failures(void){
 const enum injection modes[]={ALLOC_FAIL,FLUSH_FAIL,FINAL_GUARD,AUTHORITY_FAIL,REGISTER_FAIL,UPDATER_CLEANUP,SOURCE_CLEANUP};
 for(size_t i=0;i<sizeof modes/sizeof modes[0];i++){
  struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];select_root(o,0);
  o->injection=modes[i];struct nested_ept_result out;
  assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_FATAL);
  assert(o->atomic.poisoned&&o->registry.poisoned&&o->pool.live&&o->bindings[0].pin.live);
  if(modes[i]==UPDATER_CLEANUP||modes[i]==SOURCE_CLEANUP){
   assert(o->atomic.fault.active&&o->atomic.fault.permit.active&&o->atomic.fault.transient);
   if(modes[i]==SOURCE_CLEANUP)assert(!o->atomic.fault.permit.updater.live);
   __atomic_store_n(&f->gate.lock,0,__ATOMIC_RELEASE);__atomic_store_n(&f->catalog.lock,0,__ATOMIC_RELEASE);
   assert(!tr_atomic_fault_cleanup(&o->atomic));
   assert(!o->atomic.fault.permit.active&&o->atomic.fault.transient&&o->atomic.fault.admitted);
  }else if(modes[i]==REGISTER_FAIL){
   assert(o->atomic.fault.transient&&o->atomic.fault.admitted&&!o->atomic.fault.permit.active);
  }else assert(!o->atomic.fault.active&&!o->bridge.refill_operation&&!f->directory.operations);
  free(f);
 }
}
static void lifecycle_failures(void){
 const enum injection modes[]={FLUSH_FAIL,PROVIDER_FAIL,COMMIT_FAIL,PIN_CLEANUP};
 for(size_t i=0;i<sizeof modes/sizeof modes[0];i++){
  struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];select_root(o,0);read_write(o,false);
  struct tr_registry_slot *s=active(o);unsigned freed=o->frees;size_t live=o->pool.live;
  o->injection=modes[i];assert(!nested_ept_contexts_destroy(&o->registry));
  assert(o->registry.poisoned&&o->pool.live==live&&o->frees==freed&&s->directory.item&&
   !s->directory.forgotten&&!s->directory.record.refills&&o->bindings[0].pin.live);
  assert(!o->traces[0].usable&&!o->traces[0].registered);
  if(modes[i]==PIN_CLEANUP){
   assert(s->directory.receipt.live&&s->typed.state==TRL_RETIRED&&!s->typed.token_live);
   __atomic_store_n(&f->catalog.lock,0,__ATOMIC_RELEASE);
  }else assert(!s->directory.receipt.live);
  free(f);
 }
 struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];select_root(o,0);read_write(o,false);
 unsigned freed=o->frees;o->injection=FLUSH_FAIL;
 assert(nested_ept_contexts_invalidate(&o->registry,2,0)==NESTED_CONTEXT_FLUSH_FAILED);
 assert(o->frees==freed&&o->bindings[0].pin.live&&!o->traces[0].registered&&!o->traces[0].usable);free(f);
}
static void allocation_and_acquisition_failures(void){
 for(unsigned step=1;step<=3;step++){
  struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];
  o->fail_allocation=step;
  assert(nested_ept_contexts_select(&o->registry,f->roots[0].full_eptp)==NESTED_CONTEXT_OOM);
  assert(!o->pool.live&&!f->catalog.references&&!f->directory.operations&&!f->directory.records);
  assert(o->bindings[0].released&&!o->registry.poisoned);free(f);
 }
 for(unsigned step=1;step<=7;step++){
  struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];select_root(o,0);
  o->fail_allocation=o->allocations+step;struct nested_ept_result out;
  assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_FATAL);
  assert(o->pool.live==3&&o->traces[0].registered&&!o->atomic.fault.active&&
   active(o)->directory.record.refills==1&&!f->directory.operations);free(f);
 }
 for(unsigned step=0;step<6;step++){
  struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];select_root(o,0);
  switch(step){
  case 0:assert(ad_updater_gate_close(&f->gate,f->stamp));break;
  case 1:assert(nrc_close(&f->catalog,f->id));break;
  case 2:f->directory.next_nonce=UINT64_MAX;break;
  case 3:f->catalog.next_nonce=UINT64_MAX;break;
  case 4:f->gate.next_nonce=UINT64_MAX;break;
  case 5:active(o)->directory.record.refills=UINT64_MAX;break;
  }
  struct nested_ept_result out;
  assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_FATAL);
  assert(!o->authorities&&!o->atomic.fault.active&&!o->bridge.refill_operation&&
   !f->directory.operations&&f->catalog.references==1&&!f->gate.references&&o->pool.live==3);free(f);
 }
 for(unsigned mode=0;mode<2;mode++){
  struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];
  o->injection=mode?FLUSH_FAIL:RESERVE_FAIL;
  assert(nested_ept_contexts_select(&o->registry,f->roots[0].full_eptp)==
   (mode?NESTED_CONTEXT_FLUSH_FAILED:NESTED_CONTEXT_OOM));
  if(mode)assert(o->registry.poisoned&&o->pool.live==3&&o->bindings[0].pin.live&&
   o->slots[0].directory.record.state==TD_RETAINED);
  else assert(!o->pool.live&&!f->catalog.references&&!f->directory.operations);
  free(f);
 }
}
static void end_cleanup_failure(void){
 struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];select_root(o,0);
 o->injection=END_CLEANUP;struct nested_ept_result out;
 assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_FATAL);
 struct tr_atomic_fault *fault=&o->atomic.fault;struct td_record *record=&fault->binding->slot->directory.record;
 assert(fault->active&&fault->admitted&&!fault->transient&&!fault->permit.active&&
  o->bridge.refill_operation==&fault->operation&&o->bridge.refill_slot==fault->binding->slot&&
  o->bridge.refill_nonce==fault->nonce&&f->directory.operations==1&&record->refills==1&&
  f->catalog.references==1&&!f->gate.references);
 lock(o);record->state=TD_LIVE;record->constructing_nonce=0;unlock(o);
 assert(tr_atomic_fault_cleanup(&o->atomic));
 assert(!fault->active&&!fault->admitted&&!o->bridge.refill_operation&&!o->bridge.refill_slot&&
  !o->bridge.refill_nonce&&!f->directory.operations&&record->refills==1&&fault->fatal&&o->atomic.poisoned);
 free(f);
}
static void trace_budget_failure(void){
 struct fixture *f=calloc(1,sizeof *f);assert(f);setup_with_trace_count(f,1);struct owner *o=&f->owners[0];select_root(o,0);
 read_write(o,false);assert(nested_ept_contexts_invalidate(&o->registry,2,0)==NESTED_CONTEXT_OK);
 assert(o->traces[0].used&&!o->traces[0].usable&&!o->traces[0].registered);
 struct nested_ept_result out;
 assert(tr_atomic_fault(&o->atomic,o->registry.active,0x7000,1,&out)==TR_ATOMIC_FATAL);
 assert(o->pool.live==3&&!o->registry.active->tree.leaves&&!o->atomic.fault.active&&
  !active(o)->directory.record.refills&&o->bindings[0].pin.live);free(f);
}
static void guards(void){
 struct fixture *f=calloc(1,sizeof *f);assert(f);setup(f);struct owner *o=&f->owners[0];select_root(o,0);select_root(o,1);
 struct nested_ept_context_item *active_before=o->registry.active;uint64_t tick=o->registry.tick;
 o->bindings[0].root.generation++;
 assert(nested_ept_contexts_select(&o->registry,f->roots[0].full_eptp)==NESTED_CONTEXT_POISONED);
 assert(o->registry.active==active_before&&o->registry.tick==tick);
 struct nested_ept_result out;assert(tr_atomic_fault(&o->atomic,active_before,0x7000,1,&out)==TR_ATOMIC_REJECTED);free(f);
 f=calloc(1,sizeof *f);assert(f);setup(f);o=&f->owners[0];select_root(o,0);read_write(o,false);
 struct nested_ept_context_item *pinned=nested_ept_contexts_pin_active(&o->registry);assert(pinned);
 assert(nested_ept_contexts_invalidate(&o->registry,2,0)==NESTED_CONTEXT_POISONED);
 assert(o->traces[0].registered&&o->traces[0].usable&&o->bindings[0].pin.live);free(f);
}
int main(void){basic();fault_failures();lifecycle_failures();allocation_and_acquisition_failures();end_cleanup_failure();trace_budget_failure();guards();
 puts("PASS N41B real atomic typed contexts/pools/provider: A/B/C/A, persistent traces, first-write, receipt-before-free and failure retention");return 0;}
