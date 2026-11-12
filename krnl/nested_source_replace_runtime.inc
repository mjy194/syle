/* N47B bounded stopped WB4K/IPAT1/AD1 source replacement. All backing
 * retained; privileged VMX is owner-local. No live source mutation. */
#include "nested_source_replace_fixture.h"
#include "nested_source_replace_round.h"
#include "cache_page.h"
static struct nested_source_replace_round source_round;
static struct {
 struct nested_source_replace_fixture memory;
 struct typed_directory directory;struct typed_root_lease lease;
 struct ad_updater_gate gate[2];struct ad_entry_stamp stamp;
 uint64_t lifetime,serial,receipts,local_invalidations[2],host_root[2],host_pat[2];
 uint32_t host_audited[2];
 uint32_t lock,fault,arrived[8][2],released[8];
 bool initialized,finalized;
} atomic_runtime;
static bool atomic_failed(void){
 return __atomic_load_n(&atomic_runtime.fault,__ATOMIC_ACQUIRE)!=0||
  nested_parallel_faulted(&nested_runs[0])||nested_parallel_faulted(&nested_runs[1]);
}
static void atomic_fail(void){
 __atomic_store_n(&atomic_runtime.fault,1,__ATOMIC_RELEASE);
 /* Atomic dispatch latch closes every physical-enter query immediately.
  * markfail only attempts a bounded metadata lock; never steals a lock. */
 nested_domain_dispatch_markfail(&domain_dispatch);
}
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
 if(!atomic_meta_lock(opaque))nested_parallel_fault_stop(&nested_runs[cpu_id()],0x42b1,"N42B METADATA LOCK RETAIN");
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
 if(!a||!out||atomic_failed()||!tsr_consumer_stopped(&a->atomic)||(eptp&4095)!=0x5e)return false;
 struct {uint64_t eptp,reserved;} __attribute__((aligned(16))) descriptor={eptp,0};unsigned char fail;
 __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(descriptor),"r"(UINT64_C(1)):"cc","memory");
 if(fail||!tsr_consumer_stopped(&a->atomic))return false;
 unsigned cpu=cpu_id();atomic_runtime.local_invalidations[cpu]++;
 *out=(struct tr_local_invalidation){eptp,UINT64_C(1)<<cpu,cpu,true};return true;
}
static bool atomic_receipt(void *opaque,struct tr_registry_slot *slot,struct tr_registry_retire_scope scope,
 struct tr_registry_root_evidence *out){
 struct nested_owner_atomic *a=opaque;return a&&tr_local_provider_flush(&a->provider,slot,scope,out);
}
static bool source_current(void *context,uint64_t *out){
 (void)context;unsigned char bad;
 __asm__ volatile("vmptrst %1;setna %0":"=qm"(bad),"=m"(*out)::"cc","memory");return !bad;
}
static bool source_load(void *context,uint64_t vmcs){
 (void)context;unsigned char bad;
 __asm__ volatile("vmptrld %1;setna %0":"=qm"(bad):"m"(vmcs):"cc","memory");return !bad;
}
static bool source_read(void *context,uint64_t *out){
 (void)context;unsigned char bad;
 __asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*out):"r"(UINT64_C(0x201a)):"cc","memory");return !bad;
}
static bool source_write(void *context,uint64_t value){
 (void)context;unsigned char bad;
 __asm__ volatile("vmwrite %2,%1;setna %0":"=qm"(bad):"r"(UINT64_C(0x201a)),"r"(value):"cc","memory");return !bad;
}
static bool source_host_stable(unsigned c);
static bool source_parking(void *context){
 struct nested_parallel_runtime *r=context;
 return r&&source_host_stable(r->cpu_index)&&nested_source_replace_fixture_ready(&atomic_runtime.memory);
}
static bool source_host_read(void *unused,uint64_t h,const uint64_t **out){
 (void)unused;if(!h||(h&4095)||h>=UINT64_C(0x100000000))return false;
 *out=paging_phys_to_virt(h);return *out!=NULL;
}
static bool source_host_capture(unsigned c,uint64_t *root,uint64_t *pat){
 uint64_t cr0,cr4,flags;uint32_t lo,hi;
 __asm__ volatile("mov %%cr0,%0":"=r"(cr0));__asm__ volatile("mov %%cr3,%0":"=r"(*root));
 __asm__ volatile("mov %%cr4,%0":"=r"(cr4));__asm__ volatile("pushfq;popq %0":"=r"(flags));
 __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0x277));*pat=lo|((uint64_t)hi<<32);
 return c<2&&c==cpu_id()&&*root==paging_root()&&!(flags&0x200)&&!(cr0&UINT64_C(0x60000000))&&
  !(cr4&((UINT64_C(1)<<7)|(UINT64_C(1)<<12)|(UINT64_C(1)<<17)));
}
static bool source_host_audit(unsigned c){
 uint64_t root,pat;
 if(!source_host_capture(c,&root,&pat))return false;
 for(unsigned n=0;n<NAM_OWNED_PAGES;n++){
  uint64_t h=atomic_runtime.memory.original.owned[n];struct cache_page_lease proof;
  if(!vmx_memory_region_wb(h)||cache_page_audit_wb(root,h,pat,8192,source_host_read,NULL,&proof)!=CACHE_PAGE_AUDIT_OK)return false;
 }
 atomic_runtime.host_root[c]=root;atomic_runtime.host_pat[c]=pat;
 __atomic_store_n(&atomic_runtime.host_audited[c],1,__ATOMIC_RELEASE);return true;
}
static bool source_host_stable(unsigned c){
 uint64_t root,pat;
 return c<2&&__atomic_load_n(&atomic_runtime.host_audited[c],__ATOMIC_ACQUIRE)&&
  source_host_capture(c,&root,&pat)&&root==atomic_runtime.host_root[c]&&pat==atomic_runtime.host_pat[c];
}
static void source_fault(void *p){(void)p;atomic_fail();}
static bool source_management_guard(void *p,uint64_t nonce){
 struct nested_source_replace_round *r=p;struct tr_source_replace *m=&r->replacement;
 atomic_lock(NULL);
 bool ok=!atomic_failed()&&nonce==r->ticket.nonce&&nosr_ticket_equal(m->ticket,r->ticket)&&
  m->state==TSR_CHANGING&&m->retired==3&&!m->prepared&&!m->busy[0]&&!m->busy[1]&&
  m->old_catalog==&atomic_runtime.memory.original.catalog&&m->old_gate==&atomic_runtime.gate[0];
 atomic_unlock(NULL);
 if(!ok||!ad_updater_gate_drained(&atomic_runtime.gate[0],r->ticket.stamp))return false;
 /* Both RETIRE acknowledgments acquired before this exclusive callback.
  * VMCS references cannot change while peers only wait for PREPARE. */
 for(unsigned c=0;c<2;c++)if(source_round.refs[c].phase!=NOSR_PARKED||
  !nested_domain_dispatch_source_stopped(&domain_dispatch,c,nested_runs[c].domain_token,r->ticket.domain)||
  nested_runs[c].sparse.hardware_item)return false;
 return true;
}
static bool source_change(void *p,struct tsr_ticket t,struct nested_root_catalog *old,struct ad_updater_gate *gate,
 struct nested_root_catalog **catalog,struct ad_updater_gate **next,void *context[2]){
 struct nested_source_replace_round *r=p;
 if(!nosr_ticket_equal(t,r->ticket)||old!=&atomic_runtime.memory.original.catalog||gate!=&atomic_runtime.gate[0]||
  !source_management_guard(r,t.nonce)||
  !nested_source_replace_fixture_change(&atomic_runtime.memory,t.nonce,source_management_guard,r,4096))return false;
 struct nrc_identity id=atomic_runtime.memory.successor.identity;
 struct ad_entry_stamp stamp={id.domain,id.incarnation,id.catalog,t.stamp.epoch+1,id.lifetime};
 if(!stamp.epoch||!ad_updater_gate_init_successor(&atomic_runtime.gate[1],gate,t.stamp,stamp,4096)||
  !nested_source_replace_fixture_raw_unchanged(&atomic_runtime.memory))return false;
 *catalog=&atomic_runtime.memory.successor;*next=&atomic_runtime.gate[1];
 context[0]=context[1]=&atomic_runtime.memory;return true;
}
static bool atomic_prepare(void){
 struct nrc_identity old={0x5333,1,0x47b,1},next={0x5333,1,0x47b1,2};
 if(!nested_source_replace_fixture_prepare(&atomic_runtime.memory,&domain_memory,old,next))return false;
 atomic_runtime.stamp=(struct ad_entry_stamp){old.domain,old.incarnation,old.catalog,1,old.lifetime};
 if(!ad_updater_gate_init(&atomic_runtime.gate[0],atomic_runtime.stamp)||
  !td_init(&atomic_runtime.directory,old.domain,old.incarnation,3,1)||
  !trl_init_atomic_wb(&atomic_runtime.lease,&atomic_runtime.directory,
   (struct trl_backing){domain_memory.backing.pages[14],UINT64_C(0x47b00001),true},2*NESTED_ATOMIC_SLOTS))return false;
 source_round.dispatch=&domain_dispatch;source_round.change=source_change;source_round.change_context=&source_round;
 source_round.healthy=atomic_healthy;source_round.fault=source_fault;
 atomic_runtime.initialized=true;return true;
}
static bool source_bind(struct nested_parallel_runtime *r,bool *launch01,bool *launch02){
 struct nosr_config config={.owner=&r->atomic_catalog,.replacement=&source_round.replacement,
  .ops={source_current,source_load,source_read,source_write,source_parking},.context=r,
  .composed_eptp=&r->composed_eptp,.local_launched01=launch01,.local_launched02=launch02};
 return r->cpu_index<2&&nosr_init(&source_round.refs[r->cpu_index],&config);
}
static bool atomic_configure(struct nested_parallel_runtime *r){
 if(!r||r->cpu_index>1||!atomic_runtime.initialized)return false;
 struct nested_owner_atomic *a=&r->atomic_catalog;
 a->sparse=&r->sparse;a->vcpu=&r->vcpu;a->dispatch=&domain_dispatch;a->token=r->domain_token;
 a->lease=&atomic_runtime.lease;a->healthy=atomic_healthy;
 a->config=(struct tr_atomic_config){.catalog=&atomic_runtime.memory.original.catalog,
  .catalog_identity=atomic_runtime.memory.original.catalog.identity,.authority_context=&atomic_runtime.memory.original,
  .gate=&atomic_runtime.gate[0],.stamp=atomic_runtime.stamp,.slots=a->slots,.bindings=a->bindings,
  .slot_count=NESTED_ATOMIC_SLOTS,.traces=a->traces,.trace_count=NESTED_ATOMIC_TRACES,
  .lock=atomic_lock,.unlock=atomic_unlock,.current_cpu=atomic_cpu,
  .consumer_stopped=nested_owner_atomic_stopped,.context=a};
 a->root_provider=(struct tr_registry_root_provider){atomic_reserve,atomic_receipt};a->provider_context=a;
 a->provider=(struct tr_local_provider){.bridge=&a->bridge,.slots=a->slots,.completed=a->completed,
  .slot_count=NESTED_ATOMIC_SLOTS,.owner=r->cpu_index,.serial=&atomic_runtime.serial,
  .lock=atomic_meta_lock,.unlock=atomic_unlock,.current_cpu=atomic_cpu,.healthy=atomic_healthy,
  .invalidate=atomic_invalidate,.context=a};
 source_round.owners[r->cpu_index]=a;
 a->empty_ready=nsrr_empty_ready;a->empty_context=&source_round;
 r->source_round=&source_round;r->source_bind=source_bind;
 r->sparse.multiple=true;r->sparse.evict=true;r->sparse.atomic_catalog=a;return true;
}
static struct tr_registry_slot *atomic_active_slot(struct nested_owner_atomic *a){
 struct nested_ept_context_item *item=a->sparse->contexts.active;
 for(unsigned n=0;n<a->used;n++)if(item&&item->transaction_token==&a->slots[n].directory)return &a->slots[n];
 return NULL;
}
static uint64_t source_leaf(bool control){
 uint64_t *pt=paging_phys_to_virt(domain_memory.backing.pages[control?26:18]);
 return __atomic_load_n(&pt[15],__ATOMIC_ACQUIRE);
}
static bool source_data(unsigned phase){
 struct nested_source_replace_fixture *f=&atomic_runtime.memory;
 for(unsigned k=0;k<3;k++){
  unsigned page=k==0?14:k==1?28:27;
  const uint64_t *v=paging_phys_to_virt(domain_memory.backing.pages[page]);
  for(unsigned n=0;n<512;n++){
   uint64_t want=f->data_before[k][n];
   if(k==1&&phase>=5&&n==0x140/8)want=UINT64_C(0x47b1000000000000);
   if(k==1&&phase>=6&&n==0x180/8)want=UINT64_C(0x47b1000000000100);
   if(__atomic_load_n(&v[n],__ATOMIC_ACQUIRE)!=want)return false;
  }
 }return true;
}
static void source_root_log(struct nested_parallel_runtime *r,const char *prefix,const struct tsr_root *root){
 nested_parallel_trace_text(r,prefix);
 nested_parallel_trace_text(r,"N47B_ROOT_KEY=");nested_parallel_trace_hex(r,root->key);
 nested_parallel_trace_text(r,"N47B_ROOT_EPTP=");nested_parallel_trace_hex(r,root->eptp);
 nested_parallel_trace_text(r,"N47B_ROOT_ID=");nested_parallel_trace_hex(r,root->identity.root_id);
 nested_parallel_trace_text(r,"N47B_ROOT_INCARNATION=");nested_parallel_trace_hex(r,root->identity.root_incarnation);
 nested_parallel_trace_text(r,"N47B_ALLOCATION_LIFETIME=");nested_parallel_trace_hex(r,root->identity.allocator_lifetime);
 nested_parallel_trace_text(r,"N47B_CACHE_GENERATION=");nested_parallel_trace_hex(r,root->generation);
 nested_parallel_trace_text(r,"N47B_SOURCE_GENERATION=");nested_parallel_trace_hex(r,root->source.generation);
 nested_parallel_trace_text(r,"N47B_SOURCE_LIFETIME=");nested_parallel_trace_hex(r,root->source.lifetime);
}
static bool source_round_evidence(struct nested_parallel_runtime *r){
 struct tr_source_replace *m=&source_round.replacement;
 if(!tsr_released(m,source_round.ticket)||m->retired!=3||m->prepared!=3||
  !nrc_drained(m->old_catalog,source_round.ticket.source)||
  !ad_updater_gate_drained(m->old_gate,source_round.ticket.stamp)||
  !nested_source_replace_fixture_raw_unchanged(&atomic_runtime.memory))return false;
 for(unsigned c=0;c<2;c++){
  if(m->count[c]!=2||source_round.refs[c].phase!=NOSR_COMMITTED||source_round.refs[c].pending_live||
   !source_round.validated[c].success||!m->park[c].success||!m->restore[c].success)return false;
  nested_parallel_trace_text(r,"N47B_RETIRE_OWNER=");nested_parallel_trace_hex(r,c);
  for(size_t n=0;n<m->count[c];n++){
   const struct tsr_root *root=&m->old[c][n];struct nested_owner_atomic *a=source_round.owners[c];
   if(!tr_local_completed_matches(&a->provider,root->slot)||!root->slot->directory.forgotten)return false;
   source_root_log(r,"N47B_OLD_ROOT_RETIRED\n",root);
   size_t slot=(size_t)(root->slot-a->slots);
   if(slot>=a->used||!a->bindings[slot].released||a->bindings[slot].pin.live)return false;
   nested_parallel_trace_text(r,"N47B_RETIRE_FLUSH_SERIAL=");nested_parallel_trace_hex(r,a->completed[slot].flush_serial);
   nested_parallel_trace_text(r,"N47B_RETIRE_ACK=");nested_parallel_trace_hex(r,a->completed[slot].acked);
  }
  source_root_log(r,"N47B_NEW_ROOT_PREPARED\n",&m->fresh[c]);
  nested_parallel_trace_text(r,"N47B_VMCS01=");nested_parallel_trace_hex(r,source_round.refs[c].vmcs01);
  nested_parallel_trace_text(r,"N47B_VMCS02=");nested_parallel_trace_hex(r,source_round.refs[c].vmcs02);
  nested_parallel_trace_text(r,"N47B_PARK_EPTP=");nested_parallel_trace_hex(r,source_round.refs[c].parking_eptp);
 }
 nested_parallel_trace_text(r,"N47B_HOST_ROOT=");nested_parallel_trace_hex(r,atomic_runtime.host_root[0]);
 nested_parallel_trace_text(r,"N47B_HOST_PAT=");nested_parallel_trace_hex(r,atomic_runtime.host_pat[0]);
 nested_parallel_trace_text(r,"N47B_HOST_UNIQUE_WB_ALIASES=166\n");
 nested_parallel_trace_text(r,"N47B_DOMAIN_TICKET=");nested_parallel_trace_hex(r,source_round.ticket.domain.epoch);
 nested_parallel_trace_text(r,"N47B_SOURCE_TICKET=");nested_parallel_trace_hex(r,source_round.ticket.nonce);
 nested_parallel_trace_text(r,"N47B_OLD_CATALOG=");nested_parallel_trace_hex(r,m->old_catalog->identity.catalog);
 nested_parallel_trace_text(r,"N47B_NEW_CATALOG=");nested_parallel_trace_hex(r,m->new_catalog->identity.catalog);
 nested_parallel_trace_text(r,"N47B_SOURCE_RAW_SINGLE_CAS=1\nN47B_OLD_PINS_ENDED=4\nN47B_PARK_READBACK=2\nN47B_RESTORE_READBACK=2\nN47B_LAUNCH_UNCHANGED=2\n");
 return true;
}
static bool atomic_checkpoint(struct nested_parallel_runtime *r,unsigned phase){
 static const uint64_t keys[2][8]={{0x1005e,0x1405e,0x1005e,0x1005e,0x1405e,0x1005e,0x1005e,0x1405e},
 {0x1005e,0x1405e,0x1805e,0x1805e,0x1405e,0x1405e,0x1005e,0x1805e}};
 static const unsigned promotion[2][8][3]={
 {{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{2,1,0},{2,1,0},{2,1,0}},
 {{1,0,0},{1,1,0},{1,1,1},{1,1,1},{1,1,1},{1,1,1},{2,1,1},{2,1,1}}};
 static const unsigned created[2][8]={{1,2,2,3,4,4,4,4},{1,2,3,4,5,5,6,7}};
 if(!r||phase>=8||atomic_failed()||!nsrr_before_entry(&source_round,r->cpu_index))return false;
 unsigned cpu=r->cpu_index;struct nested_owner_atomic *a=&r->atomic_catalog;struct nested_owner_sparse *s=&r->sparse;
 uint64_t current=0,leaf=0;struct tr_registry_slot *slot=atomic_active_slot(a);
 if(!source_current(r,&current)||current!=r->vcpu.vmcs01||!nested_owner_atomic_stopped(a)||!slot||
  s->selected12!=keys[cpu][phase]||s->hardware_item!=s->contexts.active||
  !nested_owner_atomic_leaf(a,0xf000,&leaf)||(leaf&0x78)!=0x70||!(leaf&0x100)||
  a->atomic.fault.active||a->atomic.entry_active||s->creations!=created[cpu][phase]||
  /* Replacement retires both old roots and rearms only the current root. */
  s->contexts.count!=((phase==0||phase==3)?1u:2u))goto fail;
 bool writable=phase<3||(phase==5&&!cpu)||(phase==6)|| (phase==3&&cpu);
 /* New C is also private RO despite retained source D1. CPU1 p7 C refill RO. */
 if(phase==3)writable=false;
 if(!!(leaf&2)!=writable||!!(leaf&0x200)!=writable)goto fail;
 for(unsigned k=0;k<3;k++)if(a->target_promotions[k]!=promotion[cpu][phase][k])goto fail;
 if(phase==2&&!source_host_audit(cpu))goto fail;
 nested_parallel_trace_text(r,"N47B_CHECKPOINT=");nested_parallel_trace_hex(r,phase);
 nested_parallel_trace_text(r,"N47B_CHECKPOINT_CURRENT01=");nested_parallel_trace_hex(r,current);
 nested_parallel_trace_text(r,"N47B_KEY=");nested_parallel_trace_hex(r,s->selected12);
 nested_parallel_trace_text(r,"N47B_PRIVATE_EPTP=");nested_parallel_trace_hex(r,s->contexts.active->tree.eptp);
 nested_parallel_trace_text(r,"N47B_TARGET_LEAF=");nested_parallel_trace_hex(r,leaf);
 nested_parallel_trace_text(r,"N47B_TARGET_PROMOTIONS=");nested_parallel_trace_hex(r,a->target_promotions[0]+a->target_promotions[1]+a->target_promotions[2]);
 __atomic_store_n(&atomic_runtime.arrived[phase][cpu],1,__ATOMIC_RELEASE);
 if(cpu){
  if(phase==2&&!nsrr_run(&source_round,cpu))goto fail;
  return atomic_wait(&atomic_runtime.released[phase]);
 }
 if(!atomic_wait(&atomic_runtime.arrived[phase][1]))goto fail;
 if(phase==2){
  if(atomic_runtime.host_root[0]!=atomic_runtime.host_root[1]||atomic_runtime.host_pat[0]!=atomic_runtime.host_pat[1]||
   (source_leaf(false)&0x300)!=0x300||(source_leaf(true)&0x300)!=0x300||
   atomic_runtime.memory.original.catalog.references!=4||!nsrr_run(&source_round,0)||
   !atomic_wait(&source_round.committed[1])||!source_round_evidence(r))goto fail;
  atomic_runtime.stamp=atomic_runtime.gate[1].stamp;
 }else if(phase>2){
  if(!nested_source_replace_fixture_ready(&atomic_runtime.memory)||!source_data(phase)||
   (source_leaf(false)&~UINT64_C(0x300))!=0x1d073||(source_leaf(true)&~UINT64_C(0x300))!=0x1c073||
   (source_leaf(false)&0x300)!=(phase<5?0x100u:0x300u))goto fail;
 }
 struct ad_updater_gate *gate=&atomic_runtime.gate[phase>=2?1:0];
 if(!ad_updater_gate_idle(gate,atomic_runtime.stamp))goto fail;
 __atomic_store_n(&atomic_runtime.released[phase],1,__ATOMIC_RELEASE);return true;
fail:atomic_fail();return false;
}
static bool atomic_final(void){
 if(!atomic_runtime.initialized||atomic_runtime.finalized||atomic_failed()||!source_data(7)||
  source_round.replacement.state!=TSR_RELEASED||atomic_runtime.directory.records||atomic_runtime.directory.operations||
  atomic_runtime.directory.ops||atomic_runtime.directory.poisoned||atomic_runtime.lease.poisoned||
  atomic_runtime.lease.live_tokens||atomic_runtime.lease.type_epoch!=1||atomic_runtime.lease.type!=TRL_WB)return false;
 for(unsigned c=0;c<2;c++){
  struct nested_owner_atomic *a=&nested_runs[c].atomic_catalog;
  if(a->failed||a->used!=(c?7u:4u)||a->sparse->creations!=(c?7u:4u)||a->sparse->evictions!=(c?3u:0u)||
   !a->physical_ad||!a->entries||!a->ready_entries||!a->paging_faults||a->sparse->pool.live||a->sparse->pool.fault||
   source_round.refs[c].phase!=NOSR_COMMITTED||source_round.refs[c].pending_live)return false;
  for(unsigned n=0;n<a->used;n++){
   if(!tr_local_completed_matches(&a->provider,&a->slots[n])||!a->slots[n].directory.forgotten||
    !a->bindings[n].released||a->bindings[n].pin.live||a->slots[n].directory.record.refills||
    a->slots[n].directory.record.pins||a->slots[n].typed.token_live)return false;
   atomic_runtime.receipts++;
  }
  for(unsigned n=0;n<NESTED_ATOMIC_TRACES;n++)if(a->traces[n].registered||a->traces[n].usable)return false;
 }
 if(!ad_updater_gate_close(&atomic_runtime.gate[1],atomic_runtime.stamp)||
  !ad_updater_gate_drained(&atomic_runtime.gate[1],atomic_runtime.stamp)||
  !nrc_close(&atomic_runtime.memory.successor,atomic_runtime.memory.successor.identity)||
  !nrc_drained(&atomic_runtime.memory.successor,atomic_runtime.memory.successor.identity))return false;
 atomic_runtime.finalized=true;return atomic_runtime.receipts==11;
}
static bool atomic_private_inventory(void){
 uint64_t pages[2*(NESTED_OWNER_POOL_PAGES+2)];unsigned count=0;
 for(unsigned c=0;c<2;c++){
  pages[count++]=nested_runs[c].vcpu.vmcs01;pages[count++]=nested_runs[c].reserved02;
  for(unsigned n=0;n<NESTED_OWNER_POOL_PAGES;n++)pages[count++]=nested_runs[c].sparse.pages[n].hpa;
 }
 for(unsigned i=0;i<count;i++){
  if(!pages[i]||!paging_page_rw_wb(pages[i])||!vmx_memory_region_wb(pages[i]))return false;
  for(unsigned j=0;j<i;j++)if(pages[i]==pages[j])return false;
  for(unsigned j=0;j<NAM_OWNED_PAGES;j++)if(pages[i]==atomic_runtime.memory.original.owned[j])return false;
 }return nested_source_replace_fixture_ready(&atomic_runtime.memory);
}
