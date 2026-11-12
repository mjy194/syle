/* N41C: bounded WB/IPAT1/AD1 diagnostic. Fixed physical owners, immutable
 * shared source, private budget-two caches, no reset or backing reclamation. */
#include "nested_atomic_memory.h"
#include "nested_owner_atomic.h"
static struct {
 struct nested_atomic_memory memory;
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct ad_updater_gate gate;
 struct ad_entry_stamp stamp;
 uint64_t lifetime,serial;
 uint32_t lock,fault,arrived[8][2],released[8];
 uint64_t readonly_leaf,dirty_leaf,receipts;
 bool initialized,finalized;
} atomic_runtime;
static bool atomic_failed(void){return __atomic_load_n(&atomic_runtime.fault,__ATOMIC_ACQUIRE)!=0;}
static void atomic_fail(void){__atomic_store_n(&atomic_runtime.fault,1,__ATOMIC_RELEASE);}
static uint64_t atomic_clock(void){uint32_t a,d;__asm__ volatile("rdtsc":"=a"(a),"=d"(d));return (uint64_t)a|((uint64_t)d<<32);}
static bool atomic_wait(uint32_t *word){
 uint64_t start=atomic_clock();
 while(!__atomic_load_n(word,__ATOMIC_ACQUIRE)){
  if(atomic_failed()||atomic_clock()-start>UINT64_C(30000000000)){atomic_fail();return false;}
  __asm__ volatile("pause");
 }
 return !atomic_failed();
}
static bool atomic_meta_lock(void *unused){
 (void)unused;
 for(unsigned n=0;n<1048576;n++){
  uint32_t zero=0;
  if(__atomic_compare_exchange_n(&atomic_runtime.lock,&zero,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return true;
  __asm__ volatile("pause");
 }
 atomic_fail();return false;
}
static void atomic_unlock(void *unused){(void)unused;__atomic_store_n(&atomic_runtime.lock,0,__ATOMIC_RELEASE);}
static void atomic_lock(void *opaque){
 if(!atomic_meta_lock(opaque))nested_parallel_fault_stop(&nested_runs[cpu_id()],0x41c1,"N41C METADATA LOCK RETAIN");
}
static unsigned atomic_cpu(void *unused){(void)unused;return cpu_id();}
static bool atomic_healthy(void *unused){(void)unused;return !atomic_failed();}
static bool atomic_reserve(void *opaque,struct tr_registry_slot **out,uint64_t *life){
 struct nested_owner_atomic *a=opaque;
 if(!a||!out||!life||atomic_failed()||a->used==NESTED_ATOMIC_SLOTS||atomic_runtime.lifetime==UINT64_MAX)return false;
 *out=&a->slots[a->used++];*life=++atomic_runtime.lifetime;return true;
}
static bool atomic_invalidate(void *opaque,uint64_t eptp,struct tr_local_invalidation *out){
 struct nested_owner_atomic *a=opaque;
 if(!a||!out||atomic_failed()||!nested_owner_atomic_stopped(a)||(eptp&4095)!=0x5e)return false;
 struct {uint64_t eptp,reserved;} __attribute__((aligned(16))) descriptor={eptp,0};unsigned char fail;
 __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(descriptor),"r"(UINT64_C(1)):"cc","memory");
 if(fail||!nested_owner_atomic_stopped(a))return false;
 unsigned cpu=cpu_id();
 *out=(struct tr_local_invalidation){eptp,UINT64_C(1)<<cpu,cpu,true};return true;
}
static bool atomic_receipt(void *opaque,struct tr_registry_slot *slot,struct tr_registry_retire_scope scope,
 struct tr_registry_root_evidence *out){
 struct nested_owner_atomic *a=opaque;return a&&tr_local_provider_flush(&a->provider,slot,scope,out);
}
static bool atomic_prepare(void){
 struct nrc_identity id={0x5333,1,0x41c,1};
 if(!nested_atomic_memory_prepare(&atomic_runtime.memory,&domain_memory,id))return false;
 atomic_runtime.stamp=(struct ad_entry_stamp){id.domain,id.incarnation,id.catalog,1,id.lifetime};
 if(!ad_updater_gate_init(&atomic_runtime.gate,atomic_runtime.stamp)||
    !td_init(&atomic_runtime.directory,id.domain,id.incarnation,3,1)||
    !trl_init_atomic_wb(&atomic_runtime.lease,&atomic_runtime.directory,
      (struct trl_backing){domain_memory.backing.pages[14],UINT64_C(0x41c00001),true},2*NESTED_ATOMIC_SLOTS))return false;
 atomic_runtime.initialized=true;return true;
}
static bool atomic_configure(struct nested_parallel_runtime *r){
 if(!r||r->cpu_index>1||!atomic_runtime.initialized)return false;
 struct nested_owner_atomic *a=&r->atomic_catalog;
 a->sparse=&r->sparse;a->vcpu=&r->vcpu;a->dispatch=&domain_dispatch;a->token=r->domain_token;
 a->lease=&atomic_runtime.lease;a->healthy=atomic_healthy;
 a->config=(struct tr_atomic_config){.catalog=&atomic_runtime.memory.catalog,
  .catalog_identity=atomic_runtime.memory.catalog.identity,.authority_context=&atomic_runtime.memory,
  .gate=&atomic_runtime.gate,.stamp=atomic_runtime.stamp,.slots=a->slots,.bindings=a->bindings,
  .slot_count=NESTED_ATOMIC_SLOTS,.traces=a->traces,.trace_count=NESTED_ATOMIC_TRACES,
  .lock=atomic_lock,.unlock=atomic_unlock,.current_cpu=atomic_cpu,
  .consumer_stopped=nested_owner_atomic_stopped,.context=a};
 a->root_provider=(struct tr_registry_root_provider){atomic_reserve,atomic_receipt};a->provider_context=a;
 a->provider=(struct tr_local_provider){.bridge=&a->bridge,.slots=a->slots,.completed=a->completed,
  .slot_count=NESTED_ATOMIC_SLOTS,.owner=r->cpu_index,.serial=&atomic_runtime.serial,
  .lock=atomic_meta_lock,.unlock=atomic_unlock,.current_cpu=atomic_cpu,.healthy=atomic_healthy,
  .invalidate=atomic_invalidate,.context=a};
 r->sparse.multiple=true;r->sparse.evict=true;r->sparse.atomic_catalog=a;
 return true;
}
static struct tr_registry_slot *atomic_active_slot(struct nested_owner_atomic *a){
 struct nested_ept_context_item *item=a->sparse->contexts.active;
 for(unsigned n=0;n<a->used;n++)if(item&&item->transaction_token==&a->slots[n].directory)return &a->slots[n];
 return NULL;
}
static bool atomic_checkpoint(struct nested_parallel_runtime *r,unsigned phase){
 static const uint64_t keys[8]={0x1005e,0x1005e,0x1405e,0x1405e,0x1005e,0x1405e,0x1805e,0x1005e};
 if(!r||phase>=8||atomic_failed())return false;
 struct nested_owner_atomic *a=&r->atomic_catalog;struct nested_owner_sparse *s=&r->sparse;
 unsigned cpu=r->cpu_index;uint64_t leaf=0;
 struct tr_registry_slot *slot=atomic_active_slot(a);
 if(!nested_owner_atomic_stopped(a)||!slot||s->selected12!=keys[phase]||
    s->hardware_item!=s->contexts.active||!nested_owner_atomic_leaf(a,0xf000,&leaf)||
    (leaf&0x78)!=0x70||a->atomic.fault.active||a->atomic.entry_active)goto fail;
 unsigned key=phase==2||phase==3||phase==5?1:phase==6?2:0;
 uint64_t life=slot->directory.identity.allocator_lifetime;
 if(phase==0||phase==2||phase==6)a->key_lifetime[key]=life;
 if((phase==4||phase==5)&&life!=a->key_lifetime[key])goto fail;
 if(phase==4||phase==5)a->warm_checks++;
 if(phase==7&&life==a->key_lifetime[0])goto fail;
 bool writable=phase==1?cpu==0:phase==3||phase==4||phase==5||phase==6;
 if(!!(leaf&2)!=writable)goto fail;
 if(phase<6&&(s->creations!=(phase<2?1u:2u)||s->evictions))goto fail;
 if(phase>=6&&(s->creations!=(phase==6?3u:4u)||s->evictions!=(phase==6?1u:2u)))goto fail;
 if(phase==0&&(a->target_promotions[0]||a->target_promotions[1]))goto fail;
 if(phase==1&&a->target_promotions[0]!=(cpu==0?1u:0u))goto fail;
 if(phase==2&&a->target_promotions[1])goto fail;
 if(phase>=3&&a->target_promotions[1]!=1)goto fail;
 if(phase>=4&&a->target_promotions[0]!=1)goto fail;
 if(phase>=6&&a->target_promotions[2]!=1)goto fail;
 nested_parallel_trace_text(r,"N41C_CHECKPOINT=");nested_parallel_trace_hex(r,phase);
 nested_parallel_trace_text(r,"N41C_KEY=");nested_parallel_trace_hex(r,s->selected12);
 nested_parallel_trace_text(r,"N41C_PRIVATE_EPTP=");nested_parallel_trace_hex(r,s->contexts.active->tree.eptp);
 nested_parallel_trace_text(r,"N41C_ALLOCATION_LIFETIME=");nested_parallel_trace_hex(r,life);
 nested_parallel_trace_text(r,"N41C_TARGET_LEAF=");nested_parallel_trace_hex(r,leaf);
 __atomic_store_n(&atomic_runtime.arrived[phase][cpu],1,__ATOMIC_RELEASE);
 if(cpu)return atomic_wait(&atomic_runtime.released[phase]);
 if(!atomic_wait(&atomic_runtime.arrived[phase][1]))goto fail;
 /* Both owners have left their actual physical consumer and are waiting.
  * Only the BSP inspects shared evidence; source words remain atomic. */
 uint64_t *pt=paging_phys_to_virt(domain_memory.backing.pages[18]);
 uint64_t target=__atomic_load_n(&pt[15],__ATOMIC_ACQUIRE);
 if((target&0x300)!=(phase?0x300u:0x100u))goto fail;
 if(phase==0)atomic_runtime.readonly_leaf=target;
 if(phase==1)atomic_runtime.dirty_leaf=target;
 if(!ad_updater_gate_idle(&atomic_runtime.gate,atomic_runtime.stamp))goto fail;
 __atomic_store_n(&atomic_runtime.released[phase],1,__ATOMIC_RELEASE);return true;
fail:atomic_fail();return false;
}
static bool atomic_final(void){
 if(!atomic_runtime.initialized||atomic_runtime.finalized||atomic_failed()||
    atomic_runtime.readonly_leaf!=0xf173||atomic_runtime.dirty_leaf!=0xf373||
    atomic_runtime.directory.records||atomic_runtime.directory.operations||atomic_runtime.directory.ops||
    atomic_runtime.directory.poisoned||atomic_runtime.lease.poisoned||atomic_runtime.lease.live_tokens||
    atomic_runtime.lease.type_epoch!=1||atomic_runtime.lease.type!=TRL_WB)return false;
 for(unsigned cpu=0;cpu<2;cpu++){
  struct nested_owner_atomic *a=&nested_runs[cpu].atomic_catalog;
  if(a->failed||a->used!=4||!a->physical_ad||!a->entries||!a->ready_entries||!a->paging_faults||a->warm_checks!=2||
     a->atomic.fault.active||a->atomic.entry_active||a->atomic.entry_operation.live||
     a->sparse->pool.live||a->sparse->pool.fault||a->sparse->creations!=4||a->sparse->evictions!=2)return false;
  for(unsigned n=0;n<a->used;n++){
   struct tr_registry_slot *s=&a->slots[n];struct tr_atomic_binding *b=&a->bindings[n];
   if(!tr_local_completed_matches(&a->provider,s)||!s->directory.forgotten||!b->released||b->pin.live||
      s->directory.record.refills||s->directory.record.pins||s->typed.token_live||s->typed.state!=TRL_RETIRED)return false;
   atomic_runtime.receipts++;
  }
  for(unsigned n=0;n<NESTED_ATOMIC_TRACES;n++)if(a->traces[n].registered||a->traces[n].usable)return false;
  for(unsigned n=0;n<3;n++)if(a->target_promotions[n]!=1)return false;
 }
 if(!ad_updater_gate_close(&atomic_runtime.gate,atomic_runtime.stamp)||
    !ad_updater_gate_drained(&atomic_runtime.gate,atomic_runtime.stamp)||
    !nrc_close(&atomic_runtime.memory.catalog,atomic_runtime.memory.catalog.identity)||
    !nrc_drained(&atomic_runtime.memory.catalog,atomic_runtime.memory.catalog.identity))return false;
 atomic_runtime.finalized=true;return atomic_runtime.receipts==8;
}

static bool atomic_private_inventory(void){
 uint64_t pages[2*(NESTED_OWNER_POOL_PAGES+2)];unsigned count=0;
 for(unsigned cpu=0;cpu<2;cpu++){
  struct nested_parallel_runtime *r=&nested_runs[cpu];
  pages[count++]=r->vcpu.vmcs01;pages[count++]=r->reserved02;
  for(unsigned n=0;n<NESTED_OWNER_POOL_PAGES;n++)pages[count++]=r->sparse.pages[n].hpa;
 }
 for(unsigned i=0;i<count;i++){
  if(!pages[i]||!paging_page_rw_wb(pages[i])||!vmx_memory_region_wb(pages[i]))return false;
  for(unsigned j=0;j<i;j++)if(pages[i]==pages[j])return false;
  for(unsigned j=0;j<NAM_OWNED_PAGES;j++)if(pages[i]==atomic_runtime.memory.owned[j])return false;
 }
 return nested_atomic_memory_ready(&atomic_runtime.memory);
}
