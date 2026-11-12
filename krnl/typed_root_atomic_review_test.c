/* Independent N41B checks against the production bridge, pools and provider.
 * Physical CPU/INVEPT hooks below are deliberately mocks, never VMX evidence. */
#define _GNU_SOURCE
#include "typed_root_atomic.h"
#include "typed_root_local_provider.h"
#include "nested_owner_pool.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REVIEW_PAGES 64u
#define REVIEW_SLOTS 64u
#define REVIEW_TRACES 96u
#define REVIEW_DATA UINT64_C(0x8000000)
#define REVIEW_GPA UINT64_C(0xf000)
static _Alignas(4096) uint64_t review_tables[9][512];
static _Alignas(4096) unsigned char review_pages[2][REVIEW_PAGES][4096];
static _Thread_local unsigned lock_depth,physical_cpu;
struct review_fixture;
struct review_owner {
 struct review_fixture *f;
 unsigned id,reserved,allocations,frees,flushes,provider_flushes,authorities,stops;
 unsigned deny_stop,fail_flush,fail_alloc,provider_corrupt,cleanup_lock;
 bool running,provider_bad,fail_provider_lock,source_lock_after_receipt;
 struct nested_owner_pool pool;
 struct nested_sparse_page pages[REVIEW_PAGES];
 struct nested_owner_slot pool_slots[REVIEW_PAGES];
 struct typed_root_registry bridge;
 struct nested_ept_contexts registry;
 struct tr_registry_slot slots[REVIEW_SLOTS];
 struct tr_local_completed completed[REVIEW_SLOTS];
 struct tr_local_provider provider;
 struct tr_atomic_binding bindings[REVIEW_SLOTS];
 struct tr_atomic_trace traces[REVIEW_TRACES];
 struct tr_atomic_owner atomic;
};
struct review_fixture {
 pthread_mutex_t mutex,wait_mutex;
 pthread_cond_t condition;
 bool hold,held,peer_done;
 uint64_t held_version,peer_version;
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct nested_root_catalog catalog;
 struct nrc_identity id;
 struct nrc_span spans[2];
 struct nrc_root roots[3];
 struct ad_updater_gate gate;
 struct ad_entry_stamp stamp;
 uint64_t serial;
 struct review_owner owners[2];
};
static void enter(void *p) {
 struct review_owner *o=p;assert(!lock_depth);
 assert(!pthread_mutex_lock(&o->f->mutex));lock_depth=1;
}
static void leave(void *p) {
 struct review_owner *o=p;assert(lock_depth==1);lock_depth=0;
 assert(!pthread_mutex_unlock(&o->f->mutex));
}
static bool provider_lock(void *p) {
 struct review_owner *o=p;if(o->fail_provider_lock)return false;enter(p);return true;
}
static unsigned current_cpu(void *p) {(void)p;return physical_cpu;}
static bool stopped(void *p) {
 struct review_owner *o=p;assert(!lock_depth);o->stops++;
 return !o->running&&o->stops!=o->deny_stop;
}
static bool provider_healthy(void *p) {return !((struct review_owner *)p)->provider_bad;}
static bool authority(void *p,const struct nrc_span *s,uint64_t gpa,struct nrc_atomic_page *out) {
 struct review_owner *o=p;assert(!lock_depth);o->authorities++;
 uint64_t off=gpa-s->gpa;
 uint64_t *view=s->role==NRC_TABLE?&review_tables[0][0]+off/8:NULL;
 *out=(struct nrc_atomic_page){.backing={.object=s->object,.lifetime=s->lifetime,
  .hpa=s->hpa+off,.table_view=view,.permissions=s->permissions,.memory_type=6,.ignore_pat=true},
  .atomic_view=view,.pinned_writable=view!=NULL};
 return true;
}
static bool reserve(void *p,struct tr_registry_slot **slot,uint64_t *lifetime) {
 struct review_owner *o=p;assert(lock_depth==1);
 if(o->reserved==REVIEW_SLOTS)return false;
 *slot=&o->slots[o->reserved++];*lifetime=1000+o->id*REVIEW_SLOTS+o->reserved;return true;
}
static bool allocate(void *p,struct nested_sparse_page *page) {
 struct review_owner *o=p;assert(!lock_depth);o->allocations++;
 if(o->allocations==o->fail_alloc)return false;
 return nested_owner_pool_alloc(&o->pool,page);
}
static void release(void *p,struct nested_sparse_page page) {
 struct review_owner *o=p;assert(!lock_depth);enter(o);
 for(unsigned i=0;i<o->reserved;i++) {
  struct tr_registry_slot *s=&o->slots[i];
  if(s->directory.record.attached&&s->directory.record.hpa==page.hpa) {
   assert(s->directory.forgotten&&s->directory.receipt.live);
   assert(s->typed.state==TRL_RETIRED&&!s->typed.token_live);
   assert(s->receipt_version==s->directory.receipt.commit_version);
   assert(o->bindings[i].released&&!o->bindings[i].pin.live);
   assert(tr_local_completed_matches(&o->provider,s));
  }
 }
 leave(o);o->frees++;nested_owner_pool_free(&o->pool,page);assert(!o->pool.fault);
}
static bool flush(void *p,uint64_t eptp) {
 struct review_owner *o=p;assert(!lock_depth&&(eptp&4095)==0x5e);o->flushes++;
 if(o->atomic.fault.active) {
  struct tr_atomic_fault *f=&o->atomic.fault;
  assert(f->admitted&&f->transient&&f->operation.live);
  assert(f->permit.active&&f->permit.source.live&&f->permit.updater.live);
  assert(f->record&&f->record->registered&&f->record->usable);
  enter(o);assert(f->binding->slot->directory.record.refills>=2);leave(o);
  if(o->cleanup_lock==1)o->f->gate.lock=1;
  if(o->cleanup_lock==2)o->f->catalog.lock=1;
 }else{
  for(unsigned i=0;i<REVIEW_TRACES;i++) {
   struct tr_atomic_trace *t=&o->traces[i];
   if(t->used&&t->binding->slot->directory.item&&
      t->binding->slot->directory.item->tree.eptp==eptp)assert(!t->registered&&!t->usable);
  }
 }
 return o->flushes!=o->fail_flush;
}
static bool invalidate(void *p,uint64_t eptp,struct tr_local_invalidation *out) {
 struct review_owner *o=p;assert(!lock_depth&&(eptp&4095)==0x5e);o->provider_flushes++;
 *out=(struct tr_local_invalidation){.full_eptp=eptp,.acked=UINT64_C(1)<<o->id,.cpu=physical_cpu,.success=true};
 switch(o->provider_corrupt) {
 case 1:return false;case 2:out->full_eptp^=0x1000;break;
 case 3:out->cpu^=1;break;case 4:out->acked^=3;break;case 5:out->success=false;break;
 }
 if(o->provider_corrupt>=6)for(unsigned i=0;i<o->reserved;i++) {
  if(o->slots[i].directory.record.state!=TD_RETIRING)continue;
  struct tr_atomic_binding *b=&o->bindings[i];
  switch(o->provider_corrupt) {
  case 6:b->root.lifetime++;break;case 7:b->pin_nonce++;break;
  case 8:b->pin.root.source++;break;case 9:b->pin.authority_context=NULL;break;
  case 10:b->bound=false;break;case 11:b->released=true;break;
  case 12:b->root.identity.lifetime++;break;
  }
 }
 return true;
}
static bool evidence(void *p,struct tr_registry_slot *s,struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out) {
 struct review_owner *o=p;assert(!lock_depth);
 bool ok=tr_local_provider_flush(&o->provider,s,scope,out);
 if(ok&&o->source_lock_after_receipt)o->f->catalog.lock=1;
 if(ok&&o->id==0&&o->f->hold) {
  struct review_fixture *f=o->f;f->hold=false;
  enter(o);f->held_version=f->directory.version;leave(o);
  assert(!pthread_mutex_lock(&f->wait_mutex));f->held=true;assert(!pthread_cond_broadcast(&f->condition));
  while(!f->peer_done)assert(!pthread_cond_wait(&f->condition,&f->wait_mutex));
  assert(!pthread_mutex_unlock(&f->wait_mutex));
 }
 return ok;
}
static struct review_fixture *fixture(void) {
 struct review_fixture *f=calloc(1,sizeof(*f));assert(f);
 assert(!pthread_mutex_init(&f->mutex,NULL));assert(!pthread_mutex_init(&f->wait_mutex,NULL));
 assert(!pthread_cond_init(&f->condition,NULL));memset(review_tables,0,sizeof(review_tables));
 f->id=(struct nrc_identity){301,302,303,304};
 f->stamp=(struct ad_entry_stamp){301,302,303,305,304};
 assert(td_init(&f->directory,301,302,3,401));
 assert(trl_init_atomic_wb(&f->lease,&f->directory,(struct trl_backing){REVIEW_DATA,402,true},REVIEW_SLOTS*2));
 assert(nrc_init_atomic_wb(&f->catalog,f->id,48,f->spans,2,f->roots,3,authority));
 struct nrc_span s={.identity=f->id,.object=501,.lifetime=502,.gpa=0x1000,.hpa=0x4000000,
  .bytes=sizeof(review_tables),.table_view=review_tables[0],.permissions=1,.allowed_types=NRC_TYPE_WB,
  .role=NRC_TABLE,.ignore_pat=true};
 assert(nrc_add_atomic_span(&f->catalog,s,review_tables[0]));
 s=(struct nrc_span){.identity=f->id,.object=503,.lifetime=504,.gpa=0x90000,.hpa=REVIEW_DATA,
  .bytes=8192,.permissions=3,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
 assert(nrc_add_atomic_span(&f->catalog,s,NULL));
 uint64_t roots[]={0x1000,0x5000,0x6000};
 for(unsigned i=0;i<3;i++)assert(nrc_add_root(&f->catalog,(struct nrc_root){.identity=f->id,
  .source=601+i,.owner=701+i,.generation=801+i,.lifetime=901+i,
  .full_eptp=roots[i]|0x5e,.root_gpa=roots[i]}));
 review_tables[0][0]=0x2007;review_tables[1][0]=0x3007;review_tables[2][0]=0x4007;
 review_tables[3][15]=0x90073;review_tables[4][0]=0x2007;
 review_tables[5][0]=0x7007;review_tables[6][0]=0x8007;review_tables[7][0]=0x9007;review_tables[8][15]=0x91073;
 assert(nrc_publish(&f->catalog)&&ad_updater_gate_init(&f->gate,f->stamp));
 for(unsigned id=0;id<2;id++) {
  struct review_owner *o=&f->owners[id];o->f=f;o->id=id;
  for(unsigned i=0;i<REVIEW_PAGES;i++)o->pages[i]=(struct nested_sparse_page){0x10000000+id*0x100000+4096*i,review_pages[id][i]};
  assert(nested_owner_pool_init(&o->pool,id,100+id,o->pages,o->pool_slots,REVIEW_PAGES,48));
  const struct nested_sparse_ops ops={allocate,release,flush};
  assert(nested_ept_contexts_init_config(&o->registry,&ops,o,48,2,true,true,NULL,NULL));
  o->provider=(struct tr_local_provider){.bridge=&o->bridge,.slots=o->slots,.completed=o->completed,
   .slot_count=REVIEW_SLOTS,.owner=id,.serial=&f->serial,.lock=provider_lock,.unlock=leave,
   .current_cpu=current_cpu,.healthy=provider_healthy,.invalidate=invalidate,.context=o};
  const struct tr_registry_root_provider provider={reserve,evidence};
  struct tr_atomic_config config={.catalog=&f->catalog,.catalog_identity=f->id,.authority_context=o,
   .gate=&f->gate,.stamp=f->stamp,.slots=o->slots,.bindings=o->bindings,.slot_count=REVIEW_SLOTS,
   .traces=o->traces,.trace_count=REVIEW_TRACES,.lock=enter,.unlock=leave,
   .current_cpu=current_cpu,.consumer_stopped=stopped,.context=o};
  physical_cpu=id;
  assert(tr_atomic_init(&o->atomic,&o->bridge,&o->registry,&f->lease,id,&provider,o,&config));
 }
 physical_cpu=0;return f;
}
static struct tr_atomic_binding *binding(struct review_owner *o) {
 for(unsigned i=0;i<o->reserved;i++)if(o->slots[i].directory.item==o->registry.active)return &o->bindings[i];
 assert(false);return NULL;
}
static void select_root(struct review_owner *o,unsigned root) {
 physical_cpu=o->id;
 assert(nested_ept_contexts_select(&o->registry,o->f->roots[root].full_eptp)==NESTED_CONTEXT_OK);
 struct tr_atomic_binding *b=binding(o);assert(b->bound&&b->pin.live&&b->pin_nonce);
}
static struct nested_ept_result fault(struct review_owner *o,unsigned access,enum tr_atomic_status expected) {
 struct nested_ept_result out;physical_cpu=o->id;
 assert(tr_atomic_fault(&o->atomic,o->registry.active,REVIEW_GPA+123,access,&out)==expected);
 return out;
}
static void history(struct review_owner *o) {
 struct tr_registry_slot *s=binding(o)->slot;struct td_operation op;uint64_t nonce;
 enter(o);assert(td_begin(&o->f->directory,&op,&nonce));
 assert(td_resident(&o->f->directory,&op,nonce,&s->directory.record,s->directory.identity,o->id));
 assert(td_end(&o->f->directory,&op,nonce));leave(o);
}
static void clean(struct review_fixture *f) {
 for(unsigned id=0;id<2;id++) {
  struct review_owner *o=&f->owners[id];physical_cpu=id;
  assert(nested_ept_contexts_destroy(&o->registry));
  assert(!o->pool.live&&!o->atomic.fault.active);
  for(unsigned i=0;i<o->reserved;i++)if(o->slots[i].directory.record.attached)
   assert(tr_local_completed_matches(&o->provider,&o->slots[i]));
 }
 assert(!f->directory.operations&&!f->lease.live_tokens&&!f->catalog.references&&!f->gate.references);
 assert(!pthread_mutex_destroy(&f->mutex));assert(!pthread_mutex_destroy(&f->wait_mutex));
 assert(!pthread_cond_destroy(&f->condition));free(f);
}
static void shared_churn(void) {
 struct review_fixture *f=fixture();struct review_owner *a=&f->owners[0],*b=&f->owners[1];
 select_root(b,1);assert(fault(b,2,TR_ATOMIC_FILLED).hpa==REVIEW_DATA);
 assert((__atomic_load_n(&review_tables[3][15],__ATOMIC_ACQUIRE)&0x300)==0x300);
 select_root(a,0);assert(fault(a,1,TR_ATOMIC_FILLED).hpa==REVIEW_DATA);
 struct tr_atomic_binding *old=binding(a);struct nac_trace trace=a->traces[0].trace;
 assert(old->slot->directory.record.refills==1&&a->traces[0].registered);
 assert(fault(a,2,TR_ATOMIC_PROMOTED).hpa==REVIEW_DATA);history(a);
 select_root(a,1);assert(fault(a,1,TR_ATOMIC_FILLED).hpa==REVIEW_DATA);
 select_root(a,2);assert(fault(a,1,TR_ATOMIC_FILLED).hpa==REVIEW_DATA+4096);
 assert(old->released&&!old->pin.live&&old->slot->directory.forgotten);
 select_root(a,0);struct tr_atomic_binding *fresh=binding(a);
 assert(fresh!=old);
 assert(fresh->slot->directory.identity.allocator_lifetime!=trace.cache.lifetime);
 assert(fault(a,1,TR_ATOMIC_FILLED).hpa==REVIEW_DATA);
 bool reused=false;
 for(unsigned i=0;i<8;i++) {
  select_root(a,(i+1)%3);fresh=binding(a);
  for(unsigned j=0;j<a->reserved;j++)if(&a->slots[j]!=fresh->slot&&
    a->slots[j].directory.forgotten&&a->slots[j].directory.record.hpa==fresh->slot->directory.record.hpa) {
   assert(a->slots[j].directory.identity.allocator_lifetime!=fresh->slot->directory.identity.allocator_lifetime);reused=true;
  }
  assert(fault(a,1,TR_ATOMIC_FILLED).status==NESTED_EPT_OK);
 }
 assert(reused&&!f->gate.references&&f->catalog.references==3);clean(f);
 puts("PASS review actual budget-two churn, shared/distinct source paths, peer D first-write and HPA lifetime reuse");
}

static uint64_t physical_leaf(struct review_owner *o,uint64_t gpa) {
 uint64_t hpa=o->registry.active->tree.eptp&~UINT64_C(4095);
 for(unsigned level=0;level<4;level++) {
  uint64_t *view=NULL;
  for(unsigned i=0;i<REVIEW_PAGES;i++)if(o->pages[i].hpa==hpa){assert(o->pool_slots[i].live);view=o->pages[i].view;break;}
  assert(view);uint64_t raw=view[(gpa>>(39-level*9))&511];assert(raw&1);
  if(level==3)return raw;
  hpa=raw&UINT64_C(0x000ffffffffff000);
 }
 abort();
}
static void warm_identity(void) {
 for(unsigned field=0;field<17;field++) {
  struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];
  select_root(o,0);struct tr_atomic_binding *a=binding(o);struct nested_ept_context_item *old=o->registry.active;
  select_root(o,1);struct nested_ept_context_item *active=o->registry.active;
  uint64_t tick=o->registry.tick,last=old->last_used;unsigned alloc=o->allocations,flushed=o->flushes;
  switch(field) {
  case 0:a->root.identity.domain++;break;case 1:a->root.identity.incarnation++;break;
  case 2:a->root.identity.catalog++;break;case 3:a->root.identity.lifetime++;break;
  case 4:a->root.source++;break;case 5:a->root.owner++;break;case 6:a->root.generation++;break;
  case 7:a->root.lifetime++;break;case 8:a->root.full_eptp+=4096;break;case 9:a->root.root_gpa+=4096;break;
  case 10:a->pin_nonce++;break;case 11:a->pin.root.lifetime++;break;
  case 12:a->slot->directory.identity.allocator_lifetime++;break;
  case 13:old->tree.generation++;break;case 14:physical_cpu=1;break;
  case 15:o->running=true;break;case 16:assert(nrc_close(&f->catalog,f->id));break;
  }
  assert(nested_ept_contexts_select(&o->registry,0x105e)!=NESTED_CONTEXT_OK);
  assert(o->registry.active==active&&o->registry.tick==tick&&old->last_used==last);
  assert(o->allocations==alloc&&o->flushes==flushed&&!o->frees&&a->pin.live);
 }
 puts("PASS review creation-bound warm selection rejects 17 source/cache/CPU/terminal changes before LRU mutation");
}
static bool forbidden_resolve(void *p,const struct tr_registry_walk *w,uint64_t g,uint8_t n,uint64_t *h,const uint64_t **v,uint8_t *a) {
 (void)p;(void)w;(void)g;(void)n;(void)h;(void)v;(void)a;assert(false);return false;
}
static bool forbidden_guard(void *p,const struct tr_registry_walk *w) {(void)p;(void)w;assert(false);return false;}
static struct nested_ept_result forbidden_translate(void *p,const struct tr_registry_walk *w,uint64_t g,uint8_t a) {
 (void)p;(void)w;(void)g;(void)a;assert(false);return (struct nested_ept_result){0};
}
static void legacy_bypasses(void) {
 struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
 struct nested_ept_result r={.status=NESTED_EPT_OK,.l1_gpa=0x90000,.hpa=REVIEW_DATA,.effective12=3,
  .effective02=3,.leaf_resolved=true,.memory_explicit=true,.ignore_pat=true,.walk_type=6,.leaf_type=6},out;
 struct nested_ept_context_item *item=o->registry.active;unsigned calls=o->authorities,alloc=o->allocations;
 assert(typed_root_registry_fill(&o->bridge,item,REVIEW_GPA,&r)==NESTED_SPARSE_INVALID);
 assert(typed_root_registry_lookup(&o->bridge,item,REVIEW_GPA,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
 const struct nested_ept_policy policy={.maxphys=48};
 assert(typed_root_registry_miss(&o->bridge,item,REVIEW_GPA,1,forbidden_resolve,o,&policy)==NESTED_SPARSE_INVALID);
 const struct tr_registry_source source={forbidden_guard,forbidden_translate};
 assert(typed_root_registry_miss_source(&o->bridge,item,REVIEW_GPA,1,&source,o,&out)==NESTED_SPARSE_INVALID);
 assert(nested_ept_sparse_fill(&item->tree,REVIEW_GPA,&r)==NESTED_SPARSE_INVALID);
 assert(nested_ept_sparse_fill_tracked(&item->tree,REVIEW_GPA,&r,true)==NESTED_SPARSE_INVALID);
 assert(o->authorities==calls&&o->allocations==alloc&&!f->directory.operations);
 assert(nested_ept_contexts_select(&o->registry,0x101e)!=NESTED_CONTEXT_OK);
 assert(!nested_ept_contexts_set_guards(&o->registry,o->registry.guards,&o->atomic));
 (void)fault(o,1,TR_ATOMIC_FILLED);assert(!(physical_leaf(o,REVIEW_GPA)&2));
 assert(nested_ept_sparse_lookup(&item->tree,REVIEW_GPA,1,&out)==NESTED_SPARSE_BAD_LOOKUP);
 assert(!nested_ept_sparse_promote_write(&item->tree,REVIEW_GPA));
 (void)fault(o,2,TR_ATOMIC_PROMOTED);assert(physical_leaf(o,REVIEW_GPA)&2);clean(f);
 puts("PASS review old AD0 helper and direct sparse bypass exclusion; actual AD1 RO then writable leaf");
}
static void trace_identity(void) {
 for(unsigned field=0;field<23;field++) {
  struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
  (void)fault(o,1,TR_ATOMIC_FILLED);struct tr_atomic_trace *t=&o->traces[0];unsigned flushes=o->flushes,frees=o->frees;
  switch(field) {
  case 0:t->registered=false;break;case 1:t->usable=false;break;
  case 2:t->trace.source.generation++;break;case 3:t->trace.source.lifetime++;break;
  case 4:t->trace.source.identity.lifetime++;break;case 5:t->trace.cache.lifetime++;break;
  case 6:t->trace.cache.owner++;break;case 7:t->trace.cache.hpa+=4096;break;
  case 8:t->trace.cache.full_eptp+=4096;break;case 9:t->trace.cache.generation++;break;
  case 10:t->trace.path.stamp.epoch++;break;case 11:t->trace.gpa+=4096;break;
  case 12:t->trace.translation.hpa+=4096;break;case 13:t->trace.translation.l1_gpa+=4096;break;
  case 14:t->trace.translation.effective12^=4;break;case 15:t->trace.translation.effective02^=4;break;
  case 16:t->trace.translation.ignore_pat=false;break;case 17:t->trace.translation.memory_explicit=false;break;
  case 18:t->trace.translation.leaf_type=0;break;case 19:t->trace.translation.walk_type=4;break;
  case 20:t->trace.path.entry[2].pointer=(void *)1;break;
  case 21:__atomic_fetch_xor(&review_tables[2][0],UINT64_C(0x1000),__ATOMIC_ACQ_REL);break;
  case 22:t->trace.path.count=3;break;
  }
  (void)fault(o,2,TR_ATOMIC_FATAL);
  assert(!(physical_leaf(o,REVIEW_GPA)&2));
  assert(!(__atomic_load_n(&review_tables[3][15],__ATOMIC_ACQUIRE)&0x200));
  assert(o->flushes==flushes&&o->frees==frees&&o->pool.live&&o->registry.poisoned&&o->atomic.poisoned);
  assert(!o->atomic.fault.active&&!f->gate.references&&f->catalog.references==1);
 }
 puts("PASS review 23 cached-trace registration, source/cache lifetime, mapping and structural mutation rejections before write CAS");
}
static void reset_generations(void) {
 struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
 (void)fault(o,1,TR_ATOMIC_FILLED);struct tr_atomic_binding *b=binding(o);
 uint64_t source=b->root.generation,ad=f->gate.stamp.epoch,type=f->lease.type_epoch,cache=o->registry.active->tree.generation;
 assert(nested_ept_contexts_invalidate(&o->registry,1,0x105e)==NESTED_CONTEXT_OK);
 assert(o->registry.active->tree.generation==cache+1&&b->root.generation==source&&f->gate.stamp.epoch==ad&&f->lease.type_epoch==type);
 assert(!o->traces[0].usable&&!o->traces[0].registered&&b->pin.live&&!b->released&&!b->slot->directory.record.refills);
 (void)fault(o,1,TR_ATOMIC_FILLED);assert(o->traces[1].registered&&!(physical_leaf(o,REVIEW_GPA)&2));
 clean(f);
 f=fixture();o=&f->owners[0];select_root(o,0);(void)fault(o,1,TR_ATOMIC_FILLED);b=binding(o);
 struct nested_ept_context_item *p=nested_ept_contexts_pin_active(&o->registry);assert(p);
 cache=p->tree.generation;unsigned flushed=o->flushes,freed=o->frees;
 assert(nested_ept_contexts_invalidate(&o->registry,2,0)!=NESTED_CONTEXT_OK);
 assert(p->pins==1&&b->slot->directory.record.pins==1&&p->tree.generation==cache);
 assert(o->flushes==flushed&&o->frees==freed&&o->traces[0].registered&&o->traces[0].usable);
 puts("PASS review reset revocation, independent cache/source/AD/type generations and new-profile pinned-reset rejection");
}
static void failures(void) {
 for(unsigned mode=0;mode<10;mode++) {
  struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
  (void)fault(o,1,TR_ATOMIC_FILLED);struct tr_atomic_binding *b=binding(o);
  size_t live=o->pool.live;unsigned freed=o->frees;
  if(mode<5)o->provider_corrupt=mode+1;
  else if(mode==5)o->fail_provider_lock=true;
  else if(mode==6)o->fail_flush=o->flushes+1;
  else if(mode==7)o->source_lock_after_receipt=true;
  else if(mode==8)f->directory.version=UINT64_MAX;
  else b->slot->directory.record.refills++; /* unrevoked external refill */
  assert(!nested_ept_contexts_destroy(&o->registry));
  assert(o->pool.live==live&&o->frees==freed&&b->pin.live&&!b->released&&o->registry.count==1);
  assert(!b->slot->directory.forgotten&&b->slot->directory.item);
  if(mode==7) {
   assert(b->slot->directory.receipt.live&&b->slot->typed.state==TRL_RETIRED);
   f->catalog.lock=0; /* interrupted catalog owner finishes, not lock stealing */
  }else assert(!b->slot->directory.receipt.live);
  if(mode==8)assert(o->traces[0].registered&&!o->traces[0].usable);
 }
 for(unsigned mode=0;mode<2;mode++) {
  struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
  if(mode)(void)fault(o,1,TR_ATOMIC_FILLED);
  unsigned frees=o->frees;o->fail_flush=o->flushes+1;
  (void)fault(o,mode?2:1,TR_ATOMIC_FATAL);
  assert(o->frees==frees&&o->pool.live&&o->registry.active->tree.poisoned);
  assert(o->traces[0].registered&&binding(o)->slot->directory.record.refills==1);
  assert(!o->atomic.fault.active&&!f->gate.references&&f->catalog.references==1);
 }
 puts("PASS review provider/flush/receipt/source-release/refill failures retain tree and backing with no unauthorized free");
}
static void guard_cleanup_failures(void) {
 struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);o->stops=0;
 (void)fault(o,1,TR_ATOMIC_FILLED);unsigned guards=o->stops;assert(guards>10);clean(f);
 bool partial=false,published=false;
 for(unsigned n=1;n<=guards;n++) {
  f=fixture();o=&f->owners[0];select_root(o,0);o->stops=0;o->deny_stop=n;
  struct nested_ept_result out;enum tr_atomic_status status=tr_atomic_fault(&o->atomic,o->registry.active,REVIEW_GPA,1,&out);
  assert(status==TR_ATOMIC_REJECTED||status==TR_ATOMIC_FATAL);
  assert(!o->frees&&o->pool.live>=3&&!o->atomic.fault.active&&!f->gate.references&&f->catalog.references==1);
  partial|=(__atomic_load_n(&review_tables[0][0],__ATOMIC_ACQUIRE)&0x100)!=0;
  if(o->registry.active->tree.leaves) {
   published=true;assert(status==TR_ATOMIC_FATAL&&o->registry.active->tree.poisoned&&o->traces[0].registered);
  }
  if(n==guards)assert(published&&status==TR_ATOMIC_FATAL&&o->registry.active->tree.fill_count==1);
 }
 assert(partial&&published);
 for(unsigned which=1;which<=2;which++) {
  f=fixture();o=&f->owners[0];select_root(o,0);o->cleanup_lock=which;
  (void)fault(o,1,TR_ATOMIC_FATAL);struct tr_atomic_fault *op=&o->atomic.fault;
  assert(op->active&&op->admitted&&op->transient&&op->operation.live&&op->permit.active&&op->permit.source.live);
  assert(op->binding->slot->directory.record.refills==2&&o->bridge.refill_operation==&op->operation);
  assert(!o->frees&&f->catalog.references==2&&f->gate.references==(which==1));
  struct nac_ticket ticket=op->ticket;uint64_t nonce=op->nonce;
  f->gate.lock=f->catalog.lock=0; /* injected prior holders finish */
  assert(!tr_atomic_fault_cleanup(&o->atomic));
  assert(!op->permit.active&&!f->gate.references&&f->catalog.references==1);
  assert(op->active&&op->transient&&op->admitted&&o->bridge.refill_operation==&op->operation);
  assert(op->ticket.source_nonce==ticket.source_nonce&&op->ticket.updater_nonce==ticket.updater_nonce&&op->nonce==nonce);
 }
 printf("PASS review all %u local guard seams, final publication guard and both sequential NAC cleanup failures retain exact ownership\n",guards);
}
static void *thread_churn(void *p) {
 struct review_owner *o=p;physical_cpu=o->id;
 for(unsigned i=0;i<18;i++) {
  select_root(o,i%3);(void)fault(o,1,TR_ATOMIC_FILLED);history(o);
  (void)fault(o,2,TR_ATOMIC_PROMOTED);
 }
 assert(nested_ept_contexts_destroy(&o->registry));return NULL;
}
static void concurrent_churn(void) {
 for(unsigned round=0;round<12;round++) {
  struct review_fixture *f=fixture();pthread_t a,b;
  assert(!pthread_create(&a,NULL,thread_churn,&f->owners[0]));
  assert(!pthread_create(&b,NULL,thread_churn,&f->owners[1]));
  assert(!pthread_join(a,NULL)&&!pthread_join(b,NULL));
  assert(f->serial==36&&!f->directory.operations&&!f->lease.live_tokens&&!f->catalog.references&&!f->gate.references);
  for(unsigned id=0;id<2;id++) {
   struct review_owner *o=&f->owners[id];assert(o->reserved==18&&!o->pool.live);
   for(unsigned i=0;i<o->reserved;i++)assert(tr_local_completed_matches(&o->provider,&o->slots[i]));
  }
  assert(!pthread_mutex_destroy(&f->mutex));assert(!pthread_mutex_destroy(&f->wait_mutex));
 assert(!pthread_cond_destroy(&f->condition));free(f);
 }
 puts("PASS review 12 actual dual-thread owner-private churn schedules, 432 durable receipts and 864 whole atomic faults");
}
static void profile_boundaries(void) {
 struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];
 struct typed_root_registry wrong={0};const struct tr_registry_root_provider rp={reserve,evidence};
 assert(!typed_root_registry_init_root_scoped(&wrong,&f->lease,0,REVIEW_DATA,&rp,o));
 for(unsigned scope=0;scope<2;scope++) {
  struct typed_directory d;struct typed_root_lease l;struct nested_ept_contexts r;
  struct tr_atomic_owner other={0};struct typed_root_registry bridge={0};
  assert(td_init(&d,scope?301:999,scope?999:302,3,401));
  assert(trl_init_atomic_wb(&l,&d,(struct trl_backing){REVIEW_DATA,402,true},REVIEW_SLOTS));
  const struct nested_sparse_ops ops={allocate,release,flush};
  assert(nested_ept_contexts_init_config(&r,&ops,o,48,2,true,true,NULL,NULL));
  assert(!tr_atomic_init(&other,&bridge,&r,&l,0,&rp,o,&o->atomic.config));
 }
 select_root(o,0);struct tr_registry_slot *slot=binding(o)->slot;struct trl_token token;
 struct td_operation operation;uint64_t nonce;
 enter(o);assert(td_begin(&f->directory,&operation,&nonce));assert(trl_token_read(&f->lease,&slot->typed,&token));
 assert(trl_admit(&f->lease,&operation,nonce,&slot->typed,token,TRL_WB,true,true));
 assert(!trl_admit(&f->lease,&operation,nonce,&slot->typed,token,TRL_WB,true,false));
 assert(!trl_admit(&f->lease,&operation,nonce,&slot->typed,token,TRL_WB,false,true));
 assert(!trl_admit(&f->lease,&operation,nonce,&slot->typed,token,TRL_UC,true,true));
 assert(!trl_admit(&f->lease,&operation,nonce,&slot->typed,token,4,true,true));
 assert(td_end(&f->directory,&operation,nonce));leave(o);
 assert(nested_ept_contexts_destroy(&o->registry));physical_cpu=1;
 assert(nested_ept_contexts_destroy(&f->owners[1].registry));physical_cpu=0;
 struct trl_freeze frozen;assert(trl_close(&f->lease)&&trl_seal(&f->lease,&frozen));
 struct trl_transition_evidence e={.freeze=frozen,.new_epoch=402,.serial=123,
  .closed=3,.drained=3,.tlb_before=3,.cache_before=3,.tlb_after=3,.cache_after=3,
  .destination=TRL_UC,.complete_alias_audit=true,.all_refills_revoked=true,.all_roots_invalidated=true,
  .host_break_before_make=true,.all_aliases_retyped=true,.owner_stamps_ready=true};
 assert(!trl_reopen(&f->lease,frozen,&e));assert(f->lease.type==TRL_WB&&f->lease.type_epoch==401);
 const struct trl_final_evidence final={.freeze=frozen,.serial=124,.parked=3,.vmcs_gone=3,
  .refills_revoked=3,.roots_flushed=3,.caches_flushed=3,.restored_wb=true,
  .host_wb_verified=true,.complete_alias_audit=true};
 assert(!trl_backing_release_ready(&f->lease,frozen,&final)); /* no source/updater drain authority */
 clean(f);
 for(unsigned mode=0;mode<3;mode++) {
  f=fixture();o=&f->owners[0];select_root(o,0);
  uint64_t raw=__atomic_load_n(&review_tables[3][15],__ATOMIC_ACQUIRE);
  if(mode==0)raw&=~UINT64_C(0x40);
  if(mode==1)raw&=~UINT64_C(0x38);
  if(mode==2)raw=(raw&~UINT64_C(0x38))|0x20;
  __atomic_store_n(&review_tables[3][15],raw,__ATOMIC_RELEASE);
  (void)fault(o,1,TR_ATOMIC_FATAL);assert(!o->registry.active->tree.leaves&&!o->traces[0].registered);
 }
 puts("PASS review atomic WB/IPAT1/AD1 profile rejects cross-domain/old constructor, AD0, UC/WT, PAT0, type transition and legacy backing-release authority");
}
static void stale_allocation(void) {
 struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];
 bool reused=false;struct tr_atomic_trace old={0},*target=NULL;
 for(unsigned n=0;n<16;n++) {
  select_root(o,n%3);(void)fault(o,1,TR_ATOMIC_FILLED);
  for(unsigned i=0;i<REVIEW_TRACES;i++)if(o->traces[i].registered&&o->traces[i].binding==binding(o))target=&o->traces[i];
  assert(target);
  for(unsigned i=0;i<REVIEW_TRACES;i++)if(o->traces[i].used&&!o->traces[i].registered&&
    o->traces[i].trace.cache.hpa==target->trace.cache.hpa&&o->traces[i].trace.cache.lifetime!=target->trace.cache.lifetime) {
   old=o->traces[i];assert(target->trace.cache.generation==old.trace.cache.generation);reused=true;break;
  }
  if(reused)break;
 }
 assert(reused&&target);target->trace=old.trace;
 target->trace.path.entry[0].pointer=(void *)1; /* stale identity must fail before recorded pointer traversal */
 unsigned flushes=o->flushes,frees=o->frees;(void)fault(o,2,TR_ATOMIC_FATAL);
 assert(o->flushes==flushes&&o->frees==frees&&!(physical_leaf(o,REVIEW_GPA)&2));
 puts("PASS review stale trace rejected before old pointer traversal after actual root-HPA and generation-one reuse");
}
static void *held_peer(void *p) {
 struct review_owner *o=p;struct review_fixture *f=o->f;physical_cpu=o->id;
 assert(!pthread_mutex_lock(&f->wait_mutex));
 while(!f->held)assert(!pthread_cond_wait(&f->condition,&f->wait_mutex));
 assert(!pthread_mutex_unlock(&f->wait_mutex));
 for(unsigned i=0;i<4;i++){select_root(o,i%3);(void)fault(o,1,TR_ATOMIC_FILLED);history(o);(void)fault(o,2,TR_ATOMIC_PROMOTED);}
 assert(nested_ept_contexts_destroy(&o->registry));
 enter(o);f->peer_version=o->slots[o->reserved-1].receipt_version;leave(o);
 assert(!pthread_mutex_lock(&f->wait_mutex));f->peer_done=true;assert(!pthread_cond_broadcast(&f->condition));
 assert(!pthread_mutex_unlock(&f->wait_mutex));return NULL;
}
static void provider_overlap(void) {
 struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
 (void)fault(o,1,TR_ATOMIC_FILLED);history(o);struct tr_atomic_binding *retired=binding(o);
 select_root(o,1);(void)fault(o,1,TR_ATOMIC_FILLED);f->hold=true;pthread_t peer;
 assert(!pthread_create(&peer,NULL,held_peer,&f->owners[1]));select_root(o,2);assert(!pthread_join(peer,NULL));
 assert(f->held_version<f->peer_version&&f->peer_version<retired->slot->receipt_version);
 assert(retired->released&&tr_local_completed_matches(&o->provider,retired->slot));clean(f);
 puts("PASS review deterministic real-thread peer churn between bound local-provider evidence and retirement commit");
}
static void retirement_source(void) {
 struct review_fixture *f=fixture();struct review_owner *o=&f->owners[0];select_root(o,0);
 (void)fault(o,1,TR_ATOMIC_FILLED);assert(nrc_close(&f->catalog,f->id));
 clean(f); /* terminal source close blocks new work, not release of existing pins */
 f=fixture();o=&f->owners[0];select_root(o,0);(void)fault(o,1,TR_ATOMIC_FILLED);
 binding(o)->root.lifetime++;unsigned flushes=o->flushes,frees=o->frees;
 assert(!nested_ept_contexts_destroy(&o->registry));
 assert(o->flushes==flushes&&o->frees==frees&&o->traces[0].usable&&o->traces[0].registered);
 for(unsigned mode=6;mode<=12;mode++) {
  f=fixture();o=&f->owners[0];select_root(o,0);(void)fault(o,1,TR_ATOMIC_FILLED);
  struct tr_atomic_binding *b=binding(o);o->provider_corrupt=mode;frees=o->frees;size_t live=o->pool.live;
  assert(!nested_ept_contexts_destroy(&o->registry));
  assert(o->provider_flushes==1&&o->pool.live==live&&o->frees==frees);
  assert(!o->completed[0].complete&&!b->slot->directory.receipt.live&&!b->slot->directory.forgotten&&b->pin.live);
 }
 puts("PASS review terminal-close safe retirement and eight pre/post provider creation-source identity rejection points");
}
int main(int argc,char **argv) {
 const char *test=argc>1?argv[1]:"all";
 #define RUN(name) if(!strcmp(test,"all")||!strcmp(test,#name))name()
 RUN(shared_churn);RUN(warm_identity);RUN(legacy_bypasses);RUN(trace_identity);RUN(reset_generations);RUN(failures);RUN(guard_cleanup_failures);RUN(concurrent_churn);RUN(profile_boundaries);RUN(stale_allocation);RUN(provider_overlap);RUN(retirement_source);
 return 0;
}
