/* N42B production runtime, adapter, source, bridge and dispatch integration.
 * Only physical allocation/authority and privileged VMCS/INVEPT are mocked.
 * Host threads do not constitute hardware VMX acceptance. */
#define main existing_memory_review_main
#include "../efi/tests/nested41c_memory.c"
#undef main
#include "nested_owner_atomic.h"
#include "nested_owner_sparse.h"
#include <stdlib.h>
static _Alignas(4096) unsigned char private_pages[2][NESTED_OWNER_POOL_PAGES][4096];
#include <pthread.h>
#include <sched.h>
#include "nested_atomic_retrack.h"
static _Thread_local unsigned actual_cpu,depth;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
struct shared;
struct fixture {
 struct shared *shared;
 uint64_t current,physical[2],composed;
 bool launched[2],guard_probes;
 unsigned writes,reads,invepts,fail_write,fail_read,loads,fail_load,bad_read;
 uint64_t fail_invept_eptp,failed_generation;
 unsigned failed_frees;
 struct nested_ept_context_item *failed_item;
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct ad_updater_gate gate;
 struct ad_entry_stamp stamp;
 struct nested_owner_sparse sparse;
 struct nested_owner_atomic owner;
 struct nested_domain_dispatch dispatch;
 struct nested_vcpu vcpu;
 struct ept_fixture fixture;
 uint64_t serial;
 unsigned allocations,frees,flushes,provider_flushes;
 bool flush_fail,provider_fail,source_release_fail,shared_failed;
};
struct shared {
 struct fixture f[2];
 struct typed_directory directory; struct typed_root_lease lease;
 struct ad_updater_gate gate; struct nested_domain_dispatch dispatch;
 struct nested_atomic_retrack runtime; struct ad_entry_stamp outer;
 struct nested_domain_maintenance_ticket domain_ticket;
};
static void meta_lock(void *p){(void)p;assert(!depth);assert(!pthread_mutex_lock(&mutex));depth=1;}
static void meta_unlock(void *p){(void)p;assert(depth==1);depth=0;assert(!pthread_mutex_unlock(&mutex));}
static bool provider_lock(void *p){meta_lock(p);return true;}
static unsigned cpu(void *p){(void)p;return actual_cpu;}
static bool stopped(void *p){return nested_owner_atomic_stopped(&((struct fixture *)p)->owner);}
static bool healthy(void *p){struct fixture *f=p;return !f->owner.failed&&!f->shared_failed;}
static bool alloc_page(void *p,struct nested_sparse_page *page){
 struct fixture *f=p;assert(!depth);f->allocations++;return nested_owner_pool_alloc(&f->sparse.pool,page);
}
static void free_page(void *p,struct nested_sparse_page page){
 struct fixture *f=p;assert(!depth);
 /* Receipt matching reads shared directory version, including during reset. */
 meta_lock(f);
 for(unsigned n=0;n<f->owner.used;n++)if(f->owner.slots[n].directory.record.hpa==page.hpa) {
  struct tr_registry_slot *s=&f->owner.slots[n];
  assert(s->directory.forgotten&&s->directory.receipt.live&&s->typed.state==TRL_RETIRED);
  assert(f->owner.bindings[n].released&&!f->owner.bindings[n].pin.live);
  assert(tr_local_completed_matches(&f->owner.provider,s));
 }
 meta_unlock(f);
 f->frees++;nested_owner_pool_free(&f->sparse.pool,page);assert(!f->sparse.pool.fault);
}
static bool flush_page(void *p,uint64_t eptp){
 struct fixture *f=p;assert(!depth&&(eptp&4095)==0x5e);f->flushes++;
 if(f->owner.atomic.fault.active) {
  struct tr_atomic_fault *q=&f->owner.atomic.fault;
  assert(q->permit.active&&q->permit.source.live&&q->permit.updater.live&&q->transient);
 }
 return !f->flush_fail;
}
static bool reserve_slot(void *p,struct tr_registry_slot **slot,uint64_t *life){
 struct fixture *f=p;assert(depth==1);if(f->owner.used==NESTED_ATOMIC_SLOTS)return false;
 *slot=&f->owner.slots[f->owner.used++];*life=1000+f->owner.used;return true;
}
static bool invalidate(void *p,uint64_t eptp,struct tr_local_invalidation *out){
 struct fixture *f=p;assert(!depth);f->provider_flushes++;
 if(f->provider_fail)return false;
 *out=(struct tr_local_invalidation){.full_eptp=eptp,.acked=UINT64_C(1)<<actual_cpu,.cpu=actual_cpu,.success=true};return true;
}
static bool evidence(void *p,struct tr_registry_slot *s,struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out){
 struct fixture *f=p;bool ok=tr_local_provider_flush(&f->owner.provider,s,scope,out);
 if(ok&&f->source_release_fail)m.catalog.lock=1;
 return ok;
}
static bool hw_current_plain(void *p,uint64_t *v){*v=((struct fixture *)p)->current;return true;}
static bool hw_current(void *p,uint64_t *v){
 struct fixture *f=p;struct nested_owner_atomic_maintenance *ref=&f->shared->runtime.refs[actual_cpu];
 if(f->guard_probes&&!f->reads&&ref->phase==NOAM_BOUND){
  struct nested_owner_atomic_maintenance copy=*ref;copy.phase=NOAM_FRESH;copy.ops.current=hw_current_plain;
  for(unsigned field=0;field<6;field++){
   struct tam_ticket bad=ref->ticket;
   switch(field){case 0:bad.nonce++;break;case 1:bad.stamp.domain_id++;break;
    case 2:bad.stamp.incarnation++;break;case 3:bad.stamp.context_id++;break;
    case 4:bad.stamp.epoch++;break;case 5:bad.stamp.lifetime++;break;}
   assert(!noam_bind(&copy,bad,ref->domain_ticket));assert(!noam_stopped(ref,bad));
  }
  assert(noam_stopped(ref,ref->ticket));
  unsigned saved=actual_cpu;actual_cpu^=1;
  assert(!noam_stopped(ref,ref->ticket));assert(!noam_bind(&copy,ref->ticket,ref->domain_ticket));actual_cpu=saved;
  f->vcpu.busy=true;assert(!noam_stopped(ref,ref->ticket));f->vcpu.busy=false;
  f->launched[1]=false;assert(!noam_stopped(ref,ref->ticket));f->launched[1]=true;
 }
 *v=f->current;return true;
}
static bool hw_load(void *p,uint64_t v){struct fixture *f=p;assert(v==f->vcpu.vmcs01||v==f->vcpu.vmcs02);if(++f->loads==f->fail_load)return false;f->current=v;return true;}
static bool hw_read(void *p,uint64_t *v){struct fixture *f=p;if(++f->reads==f->fail_read)return false;*v=f->physical[f->current==f->vcpu.vmcs02];if(f->reads==f->bad_read)*v^=0x40;return true;}
static bool hw_write(void *p,uint64_t v){struct fixture *f=p;if(++f->writes==f->fail_write)return false;f->physical[f->current==f->vcpu.vmcs02]=v;return true;}
static bool hw_invept(void *p,uint64_t v){struct fixture *f=p;assert(actual_cpu==f->vcpu.owner_apic&&(v&4095)==0x5e);f->invepts++;
 if(v==f->fail_invept_eptp){
  for(struct nested_ept_context_item *it=f->sparse.contexts.items;it;it=it->next)if(it->tree.eptp==v)f->failed_item=it;
  assert(f->failed_item);f->failed_generation=f->failed_item->tree.generation;f->failed_frees=f->frees;return false;
 }
 return true;}
static bool parking(void *p){(void)p;return nested_atomic_memory_ready(&m);}
static struct shared *make(void){
 struct shared *g=calloc(1,sizeof(*g));assert(g);base(&d);memset(&m,0,sizeof(m));assert(nested_atomic_memory_prepare(&m,&d,id));
 struct ad_entry_stamp stamp={id.domain,id.incarnation,id.catalog,1,id.lifetime};
 assert(ad_updater_gate_init(&g->gate,stamp));assert(td_init(&g->directory,id.domain,id.incarnation,3,1));
 assert(trl_init_atomic_wb(&g->lease,&g->directory,(struct trl_backing){d.backing.pages[14],1,true},2*NESTED_ATOMIC_SLOTS));
 assert(nested_domain_dispatch_init(&g->dispatch,id.domain,id.incarnation));
 for(unsigned c=0;c<2;c++){
  actual_cpu=c;struct fixture *f=&g->f[c];f->shared=g;f->stamp=stamp;
  assert(nested_vcpu_init(&f->vcpu,c,0x500000+8192*c));assert(nested_vcpu_register02(&f->vcpu,c,0x501000+8192*c));
  f->fixture=d.backing;for(unsigned l=0;l<4;l++)f->fixture.tables[l]=d.root01[c][l];f->fixture.eptp=f->fixture.tables[0]|0x1e;
  struct nested_owner_sparse *s=&f->sparse;
  for(unsigned n=0;n<NESTED_OWNER_POOL_PAGES;n++)s->pages[n]=(struct nested_sparse_page){0x100000+0x100000*c+4096*n,private_pages[c][n]};
  assert(nested_owner_pool_init(&s->pool,c,1,s->pages,s->slots,NESTED_OWNER_POOL_PAGES,48));
  s->fixture=&f->fixture;s->initialized=true;s->multiple=true;s->evict=true;s->selected12=0x1005e;s->atomic_catalog=&f->owner;
  struct nested_owner_atomic *a=&f->owner;
  a->sparse=s;a->vcpu=&f->vcpu;a->dispatch=&g->dispatch;a->token=g->dispatch.tokens[c];a->lease=&g->lease;a->physical_ad=true;a->healthy=healthy;a->healthy_context=f;
  a->root_provider=(struct tr_registry_root_provider){reserve_slot,evidence};a->provider_context=f;
  a->config=(struct tr_atomic_config){.catalog=&m.catalog,.catalog_identity=id,.authority_context=&m,.gate=&g->gate,.stamp=stamp,.slots=a->slots,.bindings=a->bindings,.slot_count=NESTED_ATOMIC_SLOTS,.traces=a->traces,.trace_count=NESTED_ATOMIC_TRACES,.lock=meta_lock,.unlock=meta_unlock,.current_cpu=cpu,.consumer_stopped=stopped,.context=f};
  a->provider=(struct tr_local_provider){.bridge=&a->bridge,.slots=a->slots,.completed=a->completed,.slot_count=NESTED_ATOMIC_SLOTS,.owner=c,.serial=&f->serial,.lock=provider_lock,.unlock=meta_unlock,.current_cpu=cpu,.healthy=healthy,.invalidate=invalidate,.context=f};
  g->runtime.refs[c]=(struct nested_owner_atomic_maintenance){.owner=a,.maintenance=&g->runtime.maintenance,.ops={hw_current,hw_load,hw_read,hw_write,hw_invept,parking},.context=f,.composed_eptp=&f->composed,.local_launched01=&f->launched[0],.local_launched02=&f->launched[1]};
 }
 g->outer=stamp;g->runtime.dispatch=&g->dispatch;g->runtime.outer_stamp=&g->outer;
 for(unsigned c=0;c<2;c++){g->f[c].owner.empty_ready=nart_empty_ready;g->f[c].owner.empty_context=&g->runtime;}
 actual_cpu=0;return g;
}
static void populate(struct fixture *f){
 struct nested_owner_sparse *s=&f->sparse;assert(s->contexts.active);
 unsigned count=actual_cpu?3:2;
 for(unsigned n=0;n<count;n++){
  uint64_t key=0x1005e + 0x4000*n;
  if(n)assert(nested_ept_contexts_select(&s->contexts,key)==NESTED_CONTEXT_OK);
  s->selected12=key;s->hardware_item=nested_ept_contexts_pin_active(&s->contexts);assert(s->hardware_item);
  assert(nested_owner_atomic_entry(&f->owner,f->vcpu.vmcs02,s->hardware_item->tree.eptp,true));
  assert(nested_owner_atomic_fault(&f->owner,0xf000,0x181,0));assert(nested_owner_atomic_fault(&f->owner,0xf008,0x18a,0));
  if(n+1<count){assert(nested_ept_contexts_unpin(&s->contexts,s->hardware_item));s->hardware_item=0;}
 }
 f->composed=f->physical[1]=s->hardware_item->tree.eptp;f->physical[0]=f->fixture.eptp;f->current=f->vcpu.vmcs02;
 f->launched[0]=f->launched[1]=f->vcpu.launched01=f->vcpu.launched02=true;
 struct nested_domain *domain=&f->shared->dispatch.domain;unsigned c=actual_cpu;
 assert(nested_domain_vmxon(domain,c,c?0x2d000:0xd000));assert(nested_domain_load(domain,c,f->owner.token));
 struct nested_domain_cpu_token token={id.domain,id.incarnation,c};
 assert(nested_domain_guest_enter(domain,token));assert(nested_domain_guest_leave(domain,token));
}
static void *bootstrap_thread(void *p){
 struct fixture *f=p;actual_cpu=f->vcpu.owner_apic;
 const struct nested_sparse_ops ops={alloc_page,free_page,flush_page};
 assert(nested_owner_atomic_bootstrap(&f->sparse,&ops,f));return NULL;
}
static void boot(struct shared *g){
 pthread_t t[2];assert(!pthread_create(&t[0],0,bootstrap_thread,&g->f[0]));
 while(!__atomic_load_n(&g->runtime.empty[0],__ATOMIC_ACQUIRE))sched_yield();
 assert(!g->f[0].allocations&&!g->f[0].sparse.contexts.active&&!g->runtime.bootstrapped);
 assert(!pthread_create(&t[1],0,bootstrap_thread,&g->f[1]));
 for(unsigned c=0;c<2;c++)assert(!pthread_join(t[c],0));
 assert(g->runtime.empty[0]&&g->runtime.empty[1]&&g->runtime.bootstrapped);
 assert(g->runtime.maintenance.state==TAM_OPEN);
 for(unsigned c=0;c<2;c++){actual_cpu=c;populate(&g->f[c]);}
}
static void *round_thread(void *p){struct fixture *f=p;actual_cpu=f->vcpu.owner_apic;return (void *)(uintptr_t)nart_round(&f->shared->runtime,actual_cpu);}
static void cycle(unsigned mode){
 struct shared *g=make();boot(g);struct nested_atomic_retrack *r=&g->runtime;
 g->f[0].guard_probes=g->f[1].guard_probes=mode==0;
 struct fixture *f=&g->f[0];struct nested_ept_context_item *active=f->sparse.hardware_item;
 unsigned pins=active->pins;uint64_t eptp=active->tree.eptp;
 if(mode==1)f->current=f->vcpu.vmcs01;
 if(mode==2)f->physical[0]=eptp; /* Hidden VMCS01 private reference. */
 if(mode==3)f->physical[1]^=0x40; /* Same address, wrong complete EPTP. */
 if(mode==4)f->fail_write=1;
 if(mode==5)f->fail_read=5; /* Readback after successful parking write. */
 if(mode==6)f->fail_write=2;
 if(mode==7)f->fail_read=9; /* Restore readback; adjusted against primitive sequence below. */
 if(mode==8)f->fail_load=1;
 if(mode==9)f->fail_load=2;
 if(mode==10)f->fail_invept_eptp=eptp;
 if(mode==11)for(struct nested_ept_context_item *it=f->sparse.contexts.items;it;it=it->next)if(it!=active)f->fail_invept_eptp=it->tree.eptp;
 if(mode==12)f->bad_read=5;
 if(mode==13)f->bad_read=9;
 if(mode==14){assert(nrc_end(&f->owner.bindings[0].pin,f->owner.bindings[0].pin_nonce));}
 pthread_t t[2];void *result[2];assert(!pthread_create(&t[0],0,round_thread,f));
 /* The BSP must wait for AP arrival before closing normal admission. */
 while(!__atomic_load_n(&r->arrived[0],__ATOMIC_ACQUIRE))sched_yield();
 actual_cpu=1;
 assert(r->maintenance.state==TAM_OPEN&&g->dispatch.domain.gate==NESTED_DOMAIN_OPEN);
 assert(nested_owner_atomic_entry(&g->f[1].owner,g->f[1].vcpu.vmcs02,g->f[1].composed,true));
 assert(!pthread_create(&t[1],0,round_thread,&g->f[1]));
 for(unsigned c=0;c<2;c++)assert(!pthread_join(t[c],&result[c]));
 if(mode){
  assert(!result[0]&&!result[1]&&nart_failed(r)&&r->maintenance.state==TAM_POISONED);
  assert(f->sparse.pool.live&&m.catalog.references==(mode==14?3:4)&&g->outer.epoch==1);
  static const enum noam_failure failures[]={NOAM_OK,NOAM_CURRENT,NOAM_READ01,NOAM_READ02,NOAM_WRITE_PARK,NOAM_VERIFY_PARK,NOAM_WRITE_RESTORE,NOAM_VERIFY_RESTORE,NOAM_LOAD01,NOAM_LOAD02,NOAM_INVEPT,NOAM_INVEPT,NOAM_VERIFY_PARK,NOAM_VERIFY_RESTORE,NOAM_OK};
  assert(r->refs[0].failure==failures[mode]);
  if(mode==14){
   /* TAM inventory rejects missing source authority before adapter removal. */
   assert(r->refs[0].phase==NOAM_BOUND&&!f->writes&&!f->loads&&!f->invepts);
  }else assert(r->refs[0].phase==NOAM_UNCERTAIN);
  if(mode==10||mode==11){
   assert(active->pins==0&&!f->sparse.hardware_item&&!r->refs[0].pending_item);
   assert(f->failed_item&&f->failed_item->tree.generation==f->failed_generation&&f->frees==f->failed_frees);
   bool found=false;
   for(size_t n=0;n<r->maintenance.root_count[0];n++)if(r->maintenance.roots[0][n].item==f->failed_item){
    found=true;assert(!r->maintenance.receipts[0][n].success&&!r->maintenance.receipts[0][n].acked);
   }
   assert(found&&((f->failed_item==active)==(mode==10)));
  }else{
   assert(active->pins==pins);
   if(mode==6||mode==7||mode==13)assert(r->refs[0].pending_item==active&&!f->sparse.hardware_item);
   else assert(f->sparse.hardware_item==active&&!r->refs[0].pending_item);
  }
  if(mode==5||mode==12)assert(f->physical[1]==f->fixture.eptp);
  if(mode==7||mode==13)assert(f->physical[1]==eptp);
  if(mode<=5||mode==8||mode==9||mode==12||mode==14)assert(!f->frees);
  if(mode==8)assert(f->current==f->vcpu.vmcs02&&!f->writes);
  if(mode==9)assert(f->current==f->vcpu.vmcs01&&!f->writes);
 }else{
  assert(result[0]&&result[1]&&!nart_failed(r)&&r->released&&r->maintenance.state==TAM_OPEN);
  assert(g->outer.epoch==2&&r->maintenance.revoked==3&&r->maintenance.reset==3&&r->maintenance.prepared==3);
  assert(r->maintenance.page_count==9&&m.catalog.references==4&&g->lease.type_epoch==1);
  assert((__atomic_load_n(&ram[18][15],__ATOMIC_ACQUIRE)&0x300)==0);
  assert((__atomic_load_n(&ram[26][15],__ATOMIC_ACQUIRE)&0x300)==0);
  assert(g->f[1].provider_flushes==1); /* A eviction is separate from four reset receipts. */
  for(unsigned c=0;c<2;c++){
   struct fixture *x=&g->f[c];assert(x->invepts==2&&x->physical[0]==x->fixture.eptp&&x->physical[1]==x->composed);
   assert(r->refs[c].phase==NOAM_RESTORED&&x->sparse.hardware_item==r->refs[c].saved_item);
   for(unsigned n=0;n<2;n++){struct tam_root *p=&r->maintenance.roots[c][n];assert(p->item->tree.generation==p->old_generation+1);assert(r->maintenance.receipts[c][n].acked==(UINT64_C(1)<<c));}
   actual_cpu=c;assert(nested_owner_atomic_fault(&x->owner,0xf000,0x181,0));uint64_t leaf;assert(nested_owner_atomic_leaf(&x->owner,0xf000,&leaf)&&!(leaf&2));
  }
 }
 for(unsigned c=0;c<2;c++)assert(g->f[c].launched[0]&&g->f[c].launched[1]&&g->f[c].vcpu.launched01&&g->f[c].vcpu.launched02);
 static const char *names[]={"complete cycle","wrong current VMCS","hidden VMCS01 private EPTP","private EPTP low bits","park write failure","park read failure","restore write failure","restore read failure","VMPTRLD01 failure","VMPTRLD02 failure","active-root INVEPT failure","inactive-root INVEPT failure","park wrong read value","restore wrong read value","missing persistent source pin"};
 printf("PASS N42B production two-thread case %u: %s (host primitives)\n",mode,names[mode]);free(g);
}
static void guards(void){
 struct shared *g=make();boot(g);struct nested_atomic_retrack *r=&g->runtime;
 struct nested_domain_maintenance_ticket dt;
 assert(nested_domain_dispatch_maintenance_begin(&g->dispatch,NESTED_DOMAIN_AD_RESET,&dt));
 struct tam_ticket ticket={g->outer,1};struct nested_owner_atomic_maintenance *ref=&r->refs[0];
 actual_cpu=1;assert(!noam_bind(ref,ticket,dt)&&ref->phase==NOAM_FRESH);
 actual_cpu=0;assert(!noam_bind(ref,ticket,dt)); /* Unreserved ticket, real TAM still OPEN. */
 assert(!nested_domain_dispatch_consumer_stopped(&g->dispatch,0,g->f[0].owner.token));
 struct nested_domain_token token=g->f[0].owner.token;token.generation++;
 assert(!nested_domain_dispatch_maintenance_stopped(&g->dispatch,0,token,dt));
 for(unsigned n=0;n<4;n++){
  struct nested_domain_maintenance_ticket wrong=dt;
  if(n==0)wrong.domain_id++;
  if(n==1)wrong.incarnation++;
  if(n==2)wrong.epoch++;
  if(n==3)wrong.purpose=NESTED_DOMAIN_AD_OBSERVE;
  assert(!nested_domain_dispatch_maintenance_stopped(&g->dispatch,0,g->f[0].owner.token,wrong));
 }
 struct nested_domain_token good=g->f[0].owner.token;
 assert(nested_domain_dispatch_maintenance_stopped(&g->dispatch,0,good,dt));
 for(unsigned n=0;n<7;n++){
  if(n==0)g->dispatch.domain.maintenance_targets=1;
  if(n==1)g->dispatch.internal_pending[0]=true;
  if(n==2)g->dispatch.internal_resumed[0]=true;
  if(n==3)g->dispatch.internal_origin[0]=NESTED_INTERNAL_EPT;
  if(n==4)g->dispatch.domain.cpus[0].pending_ticket=1;
  if(n==5)g->dispatch.domain.objects[good.slot].prepared=true;
  if(n==6)g->dispatch.domain.objects[good.slot].running=true;
  assert(!nested_domain_dispatch_maintenance_stopped(&g->dispatch,0,good,dt));
  g->dispatch.domain.maintenance_targets=3;g->dispatch.internal_pending[0]=false;
  g->dispatch.internal_resumed[0]=false;g->dispatch.internal_origin[0]=NESTED_INTERNAL_NONE;
  g->dispatch.domain.cpus[0].pending_ticket=0;g->dispatch.domain.objects[good.slot].prepared=false;
  g->dispatch.domain.objects[good.slot].running=false;
 }
 assert(g->f[0].sparse.hardware_item->pins==1&&m.catalog.references==4);
 free(g);puts("PASS N42B actual CPU, busy, launch, complete TAM/domain ticket and token guards");
}
static void empty_selection(void){
 struct shared *g=make();actual_cpu=0;struct fixture *f=&g->f[0];
 const struct nested_sparse_ops ops={alloc_page,free_page,flush_page};
 assert(nested_owner_atomic_empty(&f->sparse,&ops,f));
 assert(!nested_owner_atomic_initial_select(&f->owner));
 assert(!f->allocations&&!f->sparse.contexts.active&&!f->owner.atomic.maintenance);
 free(g);puts("PASS N42B initial selection denied before empty two-owner binding");
}
int main(void){for(unsigned n=0;n<15;n++)cycle(n);guards();empty_selection();return 0;}
