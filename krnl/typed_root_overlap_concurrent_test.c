#define _POSIX_C_SOURCE 200809L
#include "typed_root_local_provider.h"
#include "typed_root_overlap.h"
#include "nested_owner_pool.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#define PAGES 48u
#define SLOTS 16u
#define DATA UINT64_C(0x800000)
static _Alignas(4096) unsigned char memory[2][PAGES][4096];
static _Thread_local unsigned locked,physical_cpu;
struct fixture;
struct owner {
 struct fixture *f; unsigned cpu,reserved,flushes,invalidations,publications,misses;
 struct typed_root_registry bridge; struct nested_ept_contexts registry;
 struct nested_owner_pool pool; struct nested_sparse_page pages[PAGES];
 struct nested_owner_slot pool_slots[PAGES]; struct tr_registry_slot slots[SLOTS];
 struct tr_local_completed completed[SLOTS]; struct tr_local_provider provider;
 struct tr_registry_slot *held; struct tr_registry_retire_scope scope;
 struct nested_ept_context_item *hardware_pin; struct tr_overlap_switch switched;
};
struct progress {uint64_t version,receipt_version; unsigned creates,publications,misses,receipts;};
struct fixture {
 pthread_mutex_t mutex; struct typed_directory directory; struct typed_root_lease lease;
 struct owner owner[2]; unsigned holder; uint64_t serial,held_version;
 atomic_uint held_ready,peer_done; struct progress progress; struct tr_overlap overlap;
};
static void lock(void *p){struct fixture *f=p;assert(!locked);assert(!pthread_mutex_lock(&f->mutex));locked=1;}
static void unlock(void *p){struct fixture *f=p;assert(locked);locked=0;assert(!pthread_mutex_unlock(&f->mutex));}
static bool provider_lock(void *p){struct owner *o=p;lock(o->f);return true;}
static void provider_unlock(void *p){struct owner *o=p;unlock(o->f);}
static unsigned cpu(void *p){(void)p;assert(locked);return physical_cpu;}
static bool healthy(void *p){(void)p;assert(locked);return true;}
static bool alloc(void *p,struct nested_sparse_page *out){struct owner *o=p;assert(!locked);return nested_owner_pool_alloc(&o->pool,out);}
static void release(void *p,struct nested_sparse_page page){
 struct owner *o=p;assert(!locked);
 for(unsigned n=0;n<o->reserved;n++)if(o->slots[n].directory.record.hpa==page.hpa){
  lock(o->f);assert(tr_local_completed_matches(&o->provider,&o->slots[n]));unlock(o->f);
 }
 nested_owner_pool_free(&o->pool,page);assert(!o->pool.fault);
}
static bool flush(void *p,uint64_t eptp){struct owner *o=p;assert(!locked&&(eptp&4095)==0x1e);o->flushes++;return true;}
static bool reserve(void *p,struct tr_registry_slot **out,uint64_t *life){struct owner *o=p;assert(locked&&o->reserved<SLOTS);*out=&o->slots[o->reserved++];*life=100+o->cpu*SLOTS+o->reserved;return true;}
static uint64_t now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static void wait_ready(atomic_uint *word){uint64_t start=now();assert(!locked);while(!atomic_load_explicit(word,memory_order_acquire)){assert(now()-start<UINT64_C(5000000000));sched_yield();}}
static bool invalidate(void *p,uint64_t eptp,struct tr_local_invalidation *out){
 struct owner *o=p;assert(!locked&&physical_cpu==o->cpu&&eptp==o->scope.ticket.full_eptp);o->invalidations++;
 *out=(struct tr_local_invalidation){.full_eptp=eptp,.acked=UINT64_C(1)<<physical_cpu,.cpu=physical_cpu,.success=true};return true;
}
static bool evidence(void *p,struct tr_registry_slot *s,struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out){
 struct owner *o=p;struct fixture *f=o->f;assert(!locked);o->scope=scope;
 if(!tr_local_provider_flush(&o->provider,s,scope,out))return false;
 if(o->cpu==f->holder&&!o->held){
  o->held=s;lock(f);assert(td_retire_pending(&f->directory,&o->bridge.directory.operation,scope.operation_nonce,&s->directory.record,s->directory.identity,scope.ticket));
  assert(f->directory.operations==1&&!trl_close(&f->lease));f->held_version=f->directory.version;unlock(f);
  struct tr_overlap_hold held={.completed=o->completed[s-o->slots],.version=f->held_version,.phase=f->holder,.cpu=o->cpu};
  assert(tr_overlap_publish_hold(&f->overlap,held));
  atomic_store_explicit(&f->held_ready,1,memory_order_release);wait_ready(&f->peer_done);
  assert(f->progress.creates==4&&f->progress.receipts==2&&f->progress.publications==4&&f->progress.misses==4);
  assert(f->held_version<f->progress.receipt_version&&f->progress.receipt_version<=f->progress.version);
  lock(f);assert(td_retire_pending(&f->directory,&o->bridge.directory.operation,scope.operation_nonce,&s->directory.record,s->directory.identity,scope.ticket));
  assert(f->directory.operations==1&&!trl_close(&f->lease));unlock(f);
 }
 return true;
}
static struct tr_registry_slot *active(struct owner *o){for(unsigned i=0;i<o->reserved;i++)if(&o->slots[i].directory==o->registry.active->transaction_token)return &o->slots[i];assert(0);return 0;}
static void select_use(struct owner *o,uint64_t key){
 assert(physical_cpu==o->cpu&&!locked);assert(nested_ept_contexts_select(&o->registry,key)==NESTED_CONTEXT_OK);
 struct nested_ept_context_item *item=nested_ept_contexts_pin_active(&o->registry);assert(item);
 /* Mock VMCS publication boundary: new pin exists before old unpin. */
 assert(item->pins&&item->tree.eptp);o->publications++;
 uint64_t old=o->hardware_pin?o->hardware_pin->tree.eptp:0;
 assert(tr_overlap_switch_step(&o->switched,1,item->tree.eptp,old));
 assert(tr_overlap_switch_step(&o->switched,2,item->tree.eptp,old));
 if(o->hardware_pin)assert(nested_ept_contexts_unpin(&o->registry,o->hardware_pin));
 assert(tr_overlap_switch_step(&o->switched,3,item->tree.eptp,old));
 o->hardware_pin=item;
 struct tr_registry_slot *s=active(o);struct td_operation op;uint64_t n;unsigned type;
 lock(o->f);type=o->f->lease.type;assert(td_begin(&o->f->directory,&op,&n));assert(td_resident(&o->f->directory,&op,n,&s->directory.record,s->directory.identity,o->cpu));assert(td_end(&o->f->directory,&op,n));unlock(o->f);
 struct nested_ept_result m={.status=NESTED_EPT_OK,.l1_gpa=0xf000,.hpa=DATA,.effective12=3,.effective02=3,.leaf_resolved=true,.memory_explicit=true,.leaf_type=type,.walk_type=TRL_WB,.ignore_pat=true},out;
 assert(typed_root_registry_lookup(&o->bridge,item,0xf000,3,&out)==NESTED_SPARSE_MISS);o->misses++;
 assert(typed_root_registry_fill(&o->bridge,item,0xf000,&m)==NESTED_SPARSE_OK);
 assert(typed_root_registry_lookup(&o->bridge,item,0xf000,3,&out)==NESTED_SPARSE_HIT);
}
static void *run(void *p){
 struct owner *o=p;struct fixture *f=o->f;physical_cpu=o->cpu;
 if(o->cpu==f->holder){
  select_use(o,0x1001e);select_use(o,0x1401e);select_use(o,0x1801e);
  lock(f);struct tr_registry_slot *s=o->held;assert(tr_local_completed_matches(&o->provider,s));
  struct tr_overlap_receipt final={o->completed[s-o->slots],s->directory.receipt,s->evidence_version};unlock(f);
  assert(tr_overlap_finish(&f->overlap,final));
 }
 else {
  wait_ready(&f->held_ready);lock(f);assert(f->directory.operations==1&&!trl_close(&f->lease));unlock(f);
  select_use(o,0x1001e);select_use(o,0x1401e);select_use(o,0x1801e);select_use(o,0x1001e);
  struct progress result={.creates=o->reserved,.publications=o->publications,.misses=o->misses};
  lock(f);for(unsigned i=0;i<o->reserved;i++)if(o->slots[i].directory.receipt.live){result.receipts++;assert(tr_local_completed_matches(&o->provider,&o->slots[i]));if(o->slots[i].receipt_version>result.receipt_version)result.receipt_version=o->slots[i].receipt_version;}
  result.version=f->directory.version;assert(f->directory.operations==1&&!trl_close(&f->lease));unlock(f);
  struct tr_overlap_peer peer={.before={0},.after={result.creates,result.receipts,result.misses,o->switched.completed,o->switched.sequence},.switched=o->switched,.phase=f->holder,.cpu=o->cpu};
  lock(f);for(unsigned i=0;i<o->reserved;i++)if(o->slots[i].directory.receipt.commit_version==result.receipt_version){struct tr_registry_slot *s=&o->slots[i];peer.retired=(struct tr_overlap_receipt){o->completed[i],s->directory.receipt,s->evidence_version};break;}unlock(f);
  assert(tr_overlap_publish_peer(&f->overlap,peer));
  f->progress=result;atomic_store_explicit(&f->peer_done,1,memory_order_release);
 }
 return 0;
}
static void init(struct fixture *f,unsigned holder,unsigned type){
 memset(f,0,sizeof *f);assert(!pthread_mutex_init(&f->mutex,0));atomic_init(&f->held_ready,0);atomic_init(&f->peer_done,0);f->holder=holder;
 assert(td_init(&f->directory,10,11,3,1));assert(trl_init(&f->lease,&f->directory,(struct trl_backing){DATA,99,true},2*SLOTS));
 if(type==TRL_UC){struct trl_freeze frozen;assert(trl_close(&f->lease)&&trl_seal(&f->lease,&frozen));
  struct trl_transition_evidence e={.freeze=frozen,.new_epoch=2,.serial=100,.closed=3,.drained=3,.tlb_before=3,.cache_before=3,.tlb_after=3,.cache_after=3,.destination=type,.complete_alias_audit=true,.all_refills_revoked=true,.all_roots_invalidated=true,.host_break_before_make=true,.all_aliases_retyped=true,.owner_stamps_ready=true};assert(trl_reopen(&f->lease,frozen,&e));}
 for(unsigned i=0;i<2;i++){struct owner *o=&f->owner[i];o->f=f;o->cpu=i;
  for(unsigned n=0;n<PAGES;n++)o->pages[n]=(struct nested_sparse_page){0x100000+i*0x100000+n*4096,memory[i][n]};
  assert(nested_owner_pool_init(&o->pool,i,1,o->pages,o->pool_slots,PAGES,48));const struct tr_registry_root_provider provider={reserve,evidence};
  assert(typed_root_registry_init_root_scoped(&o->bridge,&f->lease,i,DATA,&provider,o));assert(typed_root_registry_set_sync(&o->bridge,lock,unlock,f));
  o->provider=(struct tr_local_provider){.bridge=&o->bridge,.slots=o->slots,.completed=o->completed,.slot_count=SLOTS,.owner=i,.serial=&f->serial,.lock=provider_lock,.unlock=provider_unlock,.current_cpu=cpu,.healthy=healthy,.invalidate=invalidate,.context=o};
  const struct nested_sparse_ops ops={alloc,release,flush};assert(nested_ept_contexts_init_budget(&o->registry,&ops,o,48,2,true));assert(nested_ept_contexts_set_transactions(&o->registry,typed_root_registry_transactions(),&o->bridge));}
}
static void corruption_checks(const struct tr_overlap *good){
 struct tr_overlap bad;
 #define BAD(field, value) do {bad=*good;bad.field=(value);assert(!tr_overlap_valid(&bad));} while(0)
 BAD(held,0);BAD(progress,0);BAD(finished,0);BAD(hold.phase,2);BAD(hold.cpu,2);BAD(hold.version,0);
 BAD(hold.completed.scope.operation_nonce,0);BAD(hold.completed.scope.ticket.identity.domain,0);
 BAD(hold.completed.scope.ticket.identity.incarnation,0);BAD(hold.completed.scope.ticket.identity.root_id,0);
 BAD(hold.completed.scope.ticket.identity.root_incarnation,0);BAD(hold.completed.scope.ticket.identity.allocator_lifetime,0);
 BAD(hold.completed.scope.ticket.identity.owner,2);BAD(hold.completed.scope.ticket.type_epoch,0);
 BAD(hold.completed.scope.ticket.history,3);BAD(hold.completed.scope.ticket.cache_generation,0);
 BAD(hold.completed.scope.ticket.nonce,0);BAD(hold.completed.scope.ticket.hpa,0);
 BAD(hold.completed.scope.ticket.full_eptp,0);BAD(hold.completed.acked,0);BAD(hold.completed.flush_serial,0);
 BAD(hold.completed.invalidation.success,false);BAD(hold.completed.invalidation.cpu,2);BAD(hold.completed.invalidation.full_eptp,0);BAD(hold.completed.invalidation.acked,0);
 BAD(peer.retired.receipt.commit_version,good->hold.version);BAD(final.receipt.commit_version,good->peer.retired.receipt.commit_version);
 BAD(final.evidence_version,good->hold.version+1);BAD(final.completed.scope.operation_nonce,good->final.completed.scope.operation_nonce+1);
 BAD(final.completed.flush_serial,good->final.completed.flush_serial+1);BAD(final.receipt.directory,0);
 BAD(peer.retired.receipt.retirement_nonce,0);BAD(peer.retired.receipt.type_epoch,0);BAD(peer.retired.receipt.flush_serial,0);BAD(peer.retired.receipt.acked,0);
 BAD(peer.after.creations,good->peer.before.creations);BAD(peer.after.evictions,good->peer.before.evictions);BAD(peer.after.misses,good->peer.before.misses);BAD(peer.after.switches,good->peer.before.switches);
 BAD(peer.switched.old_eptp,good->peer.switched.new_eptp);BAD(peer.switched.pin,0);BAD(peer.switched.write,0);BAD(peer.switched.unpin,0);BAD(peer.switched.stage,1);
 #undef BAD
}
static void one(unsigned holder,unsigned type){
 struct fixture f;init(&f,holder,type);pthread_t threads[2];for(unsigned i=0;i<2;i++)assert(!pthread_create(&threads[i],0,run,&f.owner[i]));for(unsigned i=0;i<2;i++)assert(!pthread_join(threads[i],0));
 struct owner *h=&f.owner[holder];assert(h->held&&h->held->directory.receipt.live);assert(f.held_version<f.progress.receipt_version&&f.progress.receipt_version<h->held->receipt_version);
 assert(h->held->receipt_version==h->held->directory.receipt.commit_version&&h->held->receipt_version>h->held->evidence_version+1);
 assert(!f.directory.operations&&!f.directory.poisoned&&!f.lease.poisoned);
 assert(tr_overlap_valid(&f.overlap));
 corruption_checks(&f.overlap);
 for(unsigned i=0;i<2;i++){physical_cpu=i;struct owner *o=&f.owner[i];assert(nested_ept_contexts_unpin(&o->registry,o->hardware_pin));assert(nested_ept_contexts_destroy(&o->registry));assert(!o->pool.live);lock(&f);for(unsigned n=0;n<o->reserved;n++)assert(tr_local_completed_matches(&o->provider,&o->slots[n]));unlock(&f);}
 assert(tr_overlap_valid(&f.overlap));
 assert(!f.lease.live_tokens&&!f.directory.records&&!f.directory.operations);assert(!pthread_mutex_destroy(&f.mutex));
}
int main(void){for(unsigned repeat=0;repeat<50;repeat++){one(0,TRL_WB);one(1,TRL_UC);}puts("PASS 100 real two-thread production overlap-module/provider/registry/pool schedules; 44 corruptions per schedule rejected; stable evidence survives final destruction; physical CPU/INVEPT/VMWRITE mocked");}
