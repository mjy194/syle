/* N43: bounded WB/IPAT1/AD1 diagnostic. Fixed physical owners, immutable
 * shared source, private budget-two caches, two global resets; no backing reclamation. */
#include "nested_atomic_memory.h"
#include "nested_owner_atomic.h"
#include "nested_atomic_multiround.h"
static struct nested_atomic_retrack retrack;
static struct nested_atomic_multiround multiround={.runtime=&retrack};
static struct {
 struct nested_atomic_memory memory;
 struct typed_directory directory;
 struct typed_root_lease lease;
 struct ad_updater_gate gate;
 struct ad_entry_stamp stamp;
 uint64_t lifetime,serial;
 uint32_t lock,fault,arrived[13][2],released[13];
 uint64_t receipts,reset_receipts,source_before[2],source_after[2],tracked[2][2],changed_expected;
 uint64_t old_data[2][2][6],trace_used[2],trace_checkpoint[2];
 unsigned data_cohorts;
 struct tr_atomic_maintenance rounds[2];
 struct nested_domain_maintenance_ticket domain_rounds[2];
 uint64_t before[2][2],after[2][2],changed[2];
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
static void retrack_fault(struct nested_parallel_runtime *r){(void)r;atomic_fail();}
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
 if(!atomic_meta_lock(opaque))nested_parallel_fault_stop(&nested_runs[cpu_id()],0x42b1,"N43 METADATA LOCK RETAIN");
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
 struct nrc_identity id={0x5333,1,0x42b,1};
 if(!nested_atomic_memory_prepare(&atomic_runtime.memory,&domain_memory,id))return false;
 atomic_runtime.stamp=(struct ad_entry_stamp){id.domain,id.incarnation,id.catalog,1,id.lifetime};
 if(!ad_updater_gate_init(&atomic_runtime.gate,atomic_runtime.stamp)||
    !td_init(&atomic_runtime.directory,id.domain,id.incarnation,3,1)||
    !trl_init_atomic_wb(&atomic_runtime.lease,&atomic_runtime.directory,
      (struct trl_backing){domain_memory.backing.pages[14],UINT64_C(0x42b00001),true},2*NESTED_ATOMIC_SLOTS))return false;
 atomic_runtime.initialized=true;return true;
}
static bool retrack_current(void *context,uint64_t *out){
 (void)context;unsigned char bad;
 __asm__ volatile("vmptrst %1;setna %0":"=qm"(bad),"=m"(*out)::"cc","memory");return !bad;
}
static bool retrack_load(void *context,uint64_t vmcs){
 (void)context;unsigned char bad;
 __asm__ volatile("vmptrld %1;setna %0":"=qm"(bad):"m"(vmcs):"cc","memory");return !bad;
}
static bool retrack_read(void *context,uint64_t *out){
 (void)context;unsigned char bad;
 __asm__ volatile("vmread %2,%1;setna %0":"=qm"(bad),"=r"(*out):"r"(UINT64_C(0x201a)):"cc","memory");return !bad;
}
static bool retrack_write(void *context,uint64_t value){
 (void)context;unsigned char bad;
 __asm__ volatile("vmwrite %2,%1;setna %0":"=qm"(bad):"r"(UINT64_C(0x201a)),"r"(value):"cc","memory");return !bad;
}
static bool retrack_invept(void *context,uint64_t eptp){
 (void)context;unsigned char bad;
 struct {uint64_t eptp,reserved;} __attribute__((aligned(16))) descriptor={eptp,0};
 __asm__ volatile("invept %1,%2;setna %0":"=qm"(bad):"m"(descriptor),"r"(UINT64_C(1)):"cc","memory");return !bad;
}
static bool retrack_parking(void *context){(void)context;return nested_atomic_memory_ready(&atomic_runtime.memory);}
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
 retrack.dispatch=&domain_dispatch;retrack.outer_stamp=&atomic_runtime.stamp;
 retrack.poll=atomic_healthy;
 struct nested_owner_atomic_maintenance *ref=&retrack.refs[r->cpu_index];
 ref->owner=a;ref->context=r;
 ref->ops=(struct noam_primitives){retrack_current,retrack_load,retrack_read,retrack_write,retrack_invept,retrack_parking};
 a->empty_ready=namr_empty_ready;a->empty_context=&multiround;r->retrack_ref=ref;r->retrack_fault=retrack_fault;
 r->sparse.multiple=true;r->sparse.evict=true;r->sparse.atomic_catalog=a;
 return true;
}
static struct tr_registry_slot *atomic_active_slot(struct nested_owner_atomic *a){
 struct nested_ept_context_item *item=a->sparse->contexts.active;
 for(unsigned n=0;n<a->used;n++)if(item&&item->transaction_token==&a->slots[n].directory)return &a->slots[n];
 return NULL;
}
/* Checkpoint evidence is owner-local until the matching acquired arrival.
 * Source A/D and page contents are only inspected after BOTH guests stop. */
static unsigned retrack_key(uint64_t key){return key==0x1005e?0:key==0x1405e?1:2;}
static uint64_t retrack_source(unsigned key){
 uint64_t *pt=paging_phys_to_virt(domain_memory.backing.pages[key?26:18]);
 return __atomic_load_n(&pt[15],__ATOMIC_ACQUIRE);
}
static bool retrack_generations(unsigned cpu){
 struct nested_owner_atomic *a=&nested_runs[cpu].atomic_catalog;
 for(size_t n=0;n<retrack.maintenance.root_count[cpu];n++){
  struct tam_root *p=&retrack.maintenance.roots[cpu][n];
  if(p->item->tree.generation!=p->new_generation||p->new_generation!=p->old_generation+1||
     p->item->tree.eptp!=p->full_eptp||p->binding->slot->directory.identity.allocator_lifetime!=p->identity.allocator_lifetime||
     !p->binding->pin.live||p->binding->released||p->binding->root.generation!=p->source.generation||
     p->binding->root.lifetime!=p->source.lifetime||a->atomic.config.stamp.epoch!=atomic_runtime.stamp.epoch)return false;
 }
 return true;
}
static bool retrack_snapshot_data(bool compare){
 for(unsigned backing=0;backing<2;backing++){
  const volatile uint64_t *page=paging_phys_to_virt(domain_memory.backing.pages[backing?27:14]);
  for(unsigned cohort=0;cohort<atomic_runtime.data_cohorts;cohort++)
  for(unsigned c=0;c<2;c++)for(unsigned key=0;key<3;key++){
   unsigned n=c*3+key,offset=8+cohort*32+c*8+key;
   if(compare){if(page[offset]!=atomic_runtime.old_data[backing][cohort][n])return false;}
   else atomic_runtime.old_data[backing][cohort][n]=page[offset];
  }
 }
 return true;
}
static bool multiround_identity(struct td_identity a,struct td_identity b){
 return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
  a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static bool multiround_source(struct nrc_root a,struct nrc_root b){
 return a.identity.domain==b.identity.domain&&a.identity.incarnation==b.identity.incarnation&&
  a.identity.catalog==b.identity.catalog&&a.identity.lifetime==b.identity.lifetime&&
  a.source==b.source&&a.owner==b.owner&&a.generation==b.generation&&a.lifetime==b.lifetime&&
  a.full_eptp==b.full_eptp&&a.root_gpa==b.root_gpa;
}
static void multiround_receipt_trace(struct nested_parallel_runtime *r,const struct tam_flush_receipt *p,const struct tam_root *root){
#define N43_EVIDENCE(name,value) do{nested_parallel_trace_text(r,"N43_" name "=");nested_parallel_trace_hex(r,value);}while(0)
 N43_EVIDENCE("RESET_NONCE",p->ticket.nonce);
 N43_EVIDENCE("RESET_STAMP_DOMAIN",p->ticket.stamp.domain_id);
 N43_EVIDENCE("RESET_STAMP_INCARNATION",p->ticket.stamp.incarnation);
 N43_EVIDENCE("RESET_STAMP_CATALOG",p->ticket.stamp.context_id);
 N43_EVIDENCE("RESET_STAMP_EPOCH",p->ticket.stamp.epoch);
 N43_EVIDENCE("RESET_STAMP_LIFETIME",p->ticket.stamp.lifetime);
 N43_EVIDENCE("RESET_DOMAIN",p->identity.domain);
 N43_EVIDENCE("RESET_INCARNATION",p->identity.incarnation);
 N43_EVIDENCE("RESET_ROOT_ID",p->identity.root_id);
 N43_EVIDENCE("RESET_ROOT_INCARNATION",p->identity.root_incarnation);
 N43_EVIDENCE("RESET_LIFETIME",p->identity.allocator_lifetime);
 N43_EVIDENCE("RESET_ID_OWNER",p->identity.owner);
 N43_EVIDENCE("RESET_HPA",p->hpa);
 N43_EVIDENCE("RESET_HISTORY",p->history);
 N43_EVIDENCE("RESET_ACK",p->acked);
 N43_EVIDENCE("RESET_OLD_GENERATION",p->old_generation);
 N43_EVIDENCE("SOURCE_ID",root->source.source);
 N43_EVIDENCE("SOURCE_OWNER",root->source.owner);
 N43_EVIDENCE("SOURCE_GENERATION",root->source.generation);
 N43_EVIDENCE("SOURCE_LIFETIME",root->source.lifetime);
 N43_EVIDENCE("SOURCE_EPTP",root->source.full_eptp);
 N43_EVIDENCE("SOURCE_ROOT_GPA",root->source.root_gpa);
#undef N43_EVIDENCE
}
static bool atomic_checkpoint(struct nested_parallel_runtime *r,unsigned phase){
 static const uint64_t keys[2][13]={{0x1005e,0x1405e,0x1005e,0x1005e,0x1405e,0x1005e,0x1405e,0x1405e,0x1005e,0x1405e,0x1005e,0x1405e,0x1405e},
  {0x1005e,0x1405e,0x1805e,0x1805e,0x1405e,0x1405e,0x1805e,0x1405e,0x1805e,0x1405e,0x1405e,0x1805e,0x1405e}};
 static const unsigned promotions[2][13][3]={
  {{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{2,1,0},{2,1,0},{2,2,0},{2,2,0},{2,2,0},{3,2,0},{3,2,0},{3,3,0}},
  {{1,0,0},{1,1,0},{1,1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,2},{1,2,2},{1,2,2},{1,2,2},{1,2,2},{1,2,3},{1,3,3}}};
 if(!r||phase>=13||atomic_failed()||nart_failed(&retrack))return false;
 unsigned cpu=r->cpu_index;struct nested_owner_atomic *a=&r->atomic_catalog;struct nested_owner_sparse *s=&r->sparse;
 struct tr_registry_slot *slot=atomic_active_slot(a);uint64_t leaf=0;
 if(!nested_owner_atomic_stopped(a)||!slot||s->selected12!=keys[cpu][phase]||s->hardware_item!=s->contexts.active||
    !nested_owner_atomic_leaf(a,0xf000,&leaf)||(leaf&0x78)!=0x70||!(leaf&0x100)||a->atomic.fault.active||a->atomic.entry_active)goto fail;
 bool writable=phase<3||phase==7||phase==12||((phase==5||phase==10)&&cpu==0)||((phase==6||phase==11)&&cpu==1);
 if(!!(leaf&2)!=writable||!!(leaf&0x200)!=writable)goto fail;
 for(unsigned k=0;k<3;k++)if(a->target_promotions[k]!=promotions[cpu][phase][k])goto fail;
 unsigned created=phase==0?1:cpu&&phase>=2?3:2,evicted=cpu&&phase>=2?1:0;
 if(s->creations!=created||s->evictions!=evicted||s->contexts.count!=(phase==0?1u:2u))goto fail;
 unsigned key=retrack_key(s->selected12);uint64_t life=slot->directory.identity.allocator_lifetime;
 if(phase<2||(phase==2&&cpu))a->key_lifetime[key]=life;
 else if(life!=a->key_lifetime[key])goto fail;
 if(phase>=3&&!retrack_generations(cpu))goto fail;
 if(phase==2&&(!r->vcpu.launched01||!r->vcpu.launched02||
    !*r->retrack_ref->local_launched01||!*r->retrack_ref->local_launched02))goto fail;
 if(phase==4||phase==9){
  unsigned n=0;
  for(struct nested_ept_context_item *item=s->contexts.items;item;item=item->next){
   uint64_t target=0;
   if(n==2||!nested_owner_atomic_item_leaf(a,item,0xf000,&target)||(target&0x302)!=0x100)goto fail;
   atomic_runtime.tracked[cpu][n++]=target;
  }
  if(n!=2)goto fail;
 }
 nested_parallel_trace_text(r,"N43_CHECKPOINT=");nested_parallel_trace_hex(r,phase);
 nested_parallel_trace_text(r,"N43_KEY=");nested_parallel_trace_hex(r,s->selected12);
 nested_parallel_trace_text(r,"N43_PRIVATE_EPTP=");nested_parallel_trace_hex(r,s->contexts.active->tree.eptp);
 nested_parallel_trace_text(r,"N43_ALLOCATION_LIFETIME=");nested_parallel_trace_hex(r,life);
 nested_parallel_trace_text(r,"N43_TARGET_LEAF=");nested_parallel_trace_hex(r,leaf);
 nested_parallel_trace_text(r,"N43_CHECKPOINT_EPOCH=");nested_parallel_trace_hex(r,a->atomic.config.stamp.epoch);
 nested_parallel_trace_text(r,"N43_PRIVATE_GENERATION=");nested_parallel_trace_hex(r,s->contexts.active->tree.generation);
 for(unsigned k=0;k<3;k++){nested_parallel_trace_text(r,"N43_TARGET_PROMOTIONS=");nested_parallel_trace_hex(r,a->target_promotions[k]);}
 uint64_t used=0;
 for(unsigned n=0;n<NESTED_ATOMIC_TRACES;n++)if(a->traces[n].used)used++;
 if(used<atomic_runtime.trace_checkpoint[cpu])goto fail;
 atomic_runtime.trace_checkpoint[cpu]=used;
 nested_parallel_trace_text(r,"N43_CHECKPOINT_TRACE_USED=");nested_parallel_trace_hex(r,used);
 __atomic_store_n(&atomic_runtime.arrived[phase][cpu],1,__ATOMIC_RELEASE);
 if(cpu){
  if((phase==2||phase==7)&&!namr_round(&multiround,cpu,phase==2?1:2))goto fail;
  return atomic_wait(&atomic_runtime.released[phase]);
 }
 if(!atomic_wait(&atomic_runtime.arrived[phase][1]))goto fail;
 if(phase==2||phase==7){
  unsigned round=phase==2?0:1;
  if(round&&!retrack_snapshot_data(true))goto fail;
  atomic_runtime.data_cohorts=round+1;
  atomic_runtime.changed_expected=0;
  atomic_runtime.source_before[0]=retrack_source(0);atomic_runtime.source_before[1]=retrack_source(1);
  if((atomic_runtime.source_before[0]&0x300)!=0x300||(atomic_runtime.source_before[1]&0x300)!=0x300||
     atomic_runtime.memory.catalog.references!=4||!retrack_snapshot_data(false))goto fail;
  if(!namr_round(&multiround,0,round+1))goto fail;
  atomic_runtime.source_after[0]=retrack_source(0);atomic_runtime.source_after[1]=retrack_source(1);
  if((atomic_runtime.source_after[0]&0x300)||(atomic_runtime.source_after[1]&0x300)||
     atomic_runtime.memory.catalog.references!=4||retrack.maintenance.root_count[0]!=2||retrack.maintenance.root_count[1]!=2||
     retrack.maintenance.page_count!=9||retrack.maintenance.revoked!=3||retrack.maintenance.reset!=3||retrack.maintenance.prepared!=3||
     atomic_runtime.stamp.epoch!=round+2||!retrack_snapshot_data(true))goto fail;
  for(size_t p=0;p<retrack.maintenance.page_count;p++)for(unsigned n=0;n<512;n++){
   struct tam_page *page=&retrack.maintenance.pages[p];uint64_t raw=page->raw[n];
   if((raw&7)&&(raw&(page->depth==3?0x300:0x100)))atomic_runtime.changed_expected++;
  }
  if(retrack.maintenance.changed!=atomic_runtime.changed_expected)goto fail;
  for(unsigned c=0;c<2;c++){
   if(!retrack_generations(c))goto fail;
   for(size_t n=0;n<2;n++){
    const struct tam_root *root=&retrack.maintenance.roots[c][n];
    const struct tam_flush_receipt *receipt=&retrack.maintenance.receipts[c][n];
    if(!receipt->success||receipt->cpu!=c||receipt->acked!=(UINT64_C(1)<<c)||receipt->full_eptp!=root->full_eptp||
       receipt->hpa!=root->hpa||receipt->history!=root->history||
       !noam_ticket_equal(receipt->ticket,retrack.ticket)||!multiround_identity(receipt->identity,root->identity)||
       receipt->old_generation!=root->old_generation||receipt->new_generation!=root->new_generation)goto fail;
    if(round){
     const struct tam_root *old=&atomic_runtime.rounds[0].roots[c][n];
     if(!multiround_identity(root->identity,old->identity)||root->full_eptp!=old->full_eptp||root->hpa!=old->hpa||
        root->binding!=old->binding||root->item!=old->item||root->history!=old->history||
        root->old_generation!=old->new_generation||!multiround_source(root->source,old->source))goto fail;
    }
    multiround_receipt_trace(r,receipt,root);
    atomic_runtime.reset_receipts++;
    nested_parallel_trace_text(r,"N43_RESET_EPTP=");nested_parallel_trace_hex(r,receipt->full_eptp);
    nested_parallel_trace_text(r,"N43_RESET_OWNER=");nested_parallel_trace_hex(r,c);
    nested_parallel_trace_text(r,"N43_RESET_GENERATION=");nested_parallel_trace_hex(r,receipt->new_generation);
   }
  }
  if(round&&(retrack.ticket.nonce<=atomic_runtime.rounds[0].ticket.nonce||
     retrack.domain_ticket.epoch<=atomic_runtime.domain_rounds[0].epoch))goto fail;
  /* Volatile byte copy keeps this large evidence snapshot freestanding. */
  for(size_t n=0;n<sizeof(retrack.maintenance);n++)
   ((volatile unsigned char *)&atomic_runtime.rounds[round])[n]=((const volatile unsigned char *)&retrack.maintenance)[n];
  atomic_runtime.domain_rounds[round]=retrack.domain_ticket;
  atomic_runtime.changed[round]=atomic_runtime.changed_expected;
  for(unsigned n=0;n<2;n++){
   atomic_runtime.before[round][n]=atomic_runtime.source_before[n];
   atomic_runtime.after[round][n]=atomic_runtime.source_after[n];
   nested_parallel_trace_text(r,"N43_SOURCE_BEFORE=");nested_parallel_trace_hex(r,atomic_runtime.before[round][n]);
   nested_parallel_trace_text(r,"N43_SOURCE_AFTER=");nested_parallel_trace_hex(r,atomic_runtime.after[round][n]);
  }
  nested_parallel_trace_text(r,"N43_ROUND_COMMITTED=");nested_parallel_trace_hex(r,round+1);
  nested_parallel_trace_text(r,"N43_SOURCE_A0D0=2\nN43_SOURCE_PINS=4\n");
  nested_parallel_trace_text(r,"N43_AD_EPOCH=");nested_parallel_trace_hex(r,atomic_runtime.stamp.epoch);
  nested_parallel_trace_text(r,"N43_DOMAIN_TICKET=");nested_parallel_trace_hex(r,retrack.domain_ticket.epoch);
  nested_parallel_trace_text(r,"N43_TAM_TICKET=");nested_parallel_trace_hex(r,retrack.ticket.nonce);
  nested_parallel_trace_text(r,"N43_CHANGED_CAS=");nested_parallel_trace_hex(r,retrack.maintenance.changed);
 }else{
  uint64_t ab=retrack_source(0),c=retrack_source(1);
  if(phase<2){if((ab&0x300)!=0x300)goto fail;}
  if(phase==3||phase==4||phase==8||phase==9){if((ab&0x300)!=0x100||(c&0x300)!=0x100||!retrack_snapshot_data(true))goto fail;}
  if((phase==5||phase==10)&&((ab&0x300)!=0x300||(c&0x300)!=0x100))goto fail;
  if((phase==6||phase==11||phase==12)&&((ab&0x300)!=0x300||(c&0x300)!=0x300))goto fail;
 }
 if(phase>=2&&atomic_runtime.memory.catalog.references!=4)goto fail;
 if(phase>=3&&!retrack_snapshot_data(true))goto fail;
 if(!ad_updater_gate_idle(&atomic_runtime.gate,atomic_runtime.stamp))goto fail;
 if(phase>=2){
  uint64_t generation=phase<7?1:2;
  if(__atomic_load_n(&multiround.released,__ATOMIC_ACQUIRE)!=generation||
     __atomic_load_n(&multiround.completed[0],__ATOMIC_ACQUIRE)!=generation||
     __atomic_load_n(&multiround.completed[1],__ATOMIC_ACQUIRE)!=generation)goto fail;
  for(unsigned c=0;c<2;c++)if(retrack.refs[c].phase!=NOAM_COMMITTED||retrack.refs[c].pending_item||
     retrack.refs[c].failure!=NOAM_OK)goto fail;
 }
 nested_parallel_trace_text(r,"N43_SOURCE_AB=");nested_parallel_trace_hex(r,retrack_source(0));
 nested_parallel_trace_text(r,"N43_SOURCE_C=");nested_parallel_trace_hex(r,retrack_source(1));
 __atomic_store_n(&atomic_runtime.released[phase],1,__ATOMIC_RELEASE);return true;
fail:atomic_fail();return false;
}
static bool atomic_final(void){
 if(!atomic_runtime.initialized||atomic_runtime.finalized||atomic_failed()||nart_failed(&retrack)||
    __atomic_load_n(&multiround.released,__ATOMIC_ACQUIRE)!=2||
    __atomic_load_n(&multiround.completed[0],__ATOMIC_ACQUIRE)!=2||
    __atomic_load_n(&multiround.completed[1],__ATOMIC_ACQUIRE)!=2||atomic_runtime.reset_receipts!=8||
    retrack.maintenance.state!=TAM_OPEN||atomic_runtime.stamp.epoch!=3||
    atomic_runtime.directory.records||atomic_runtime.directory.operations||atomic_runtime.directory.ops||
    atomic_runtime.directory.poisoned||atomic_runtime.lease.poisoned||atomic_runtime.lease.live_tokens||
    atomic_runtime.lease.type_epoch!=1||atomic_runtime.lease.type!=TRL_WB)return false;
 for(unsigned cpu=0;cpu<2;cpu++){
  struct nested_owner_atomic *a=&nested_runs[cpu].atomic_catalog;
  if(a->failed||a->used!=(cpu?3u:2u)||!a->physical_ad||!a->entries||!a->ready_entries||!a->paging_faults||
     a->atomic.fault.active||a->atomic.entry_active||a->atomic.entry_operation.live||
     a->sparse->pool.live||a->sparse->pool.fault||a->sparse->creations!=(cpu?3u:2u)||a->sparse->evictions!=(cpu?1u:0u))return false;
  if(retrack.refs[cpu].phase!=NOAM_COMMITTED||retrack.refs[cpu].pending_item||retrack.refs[cpu].failure!=NOAM_OK)return false;
  for(unsigned n=0;n<a->used;n++){
   struct tr_registry_slot *s=&a->slots[n];struct tr_atomic_binding *b=&a->bindings[n];
   if(!tr_local_completed_matches(&a->provider,s)||!s->directory.forgotten||!b->released||b->pin.live||
      s->directory.record.refills||s->directory.record.pins||s->typed.token_live||s->typed.state!=TRL_RETIRED)return false;
   atomic_runtime.receipts++;
  }
  for(unsigned n=0;n<NESTED_ATOMIC_TRACES;n++){
   if(a->traces[n].registered||a->traces[n].usable)return false;
   if(a->traces[n].used)atomic_runtime.trace_used[cpu]++;
  }
  if(!atomic_runtime.trace_used[cpu]||atomic_runtime.trace_used[cpu]<atomic_runtime.trace_checkpoint[cpu]||atomic_runtime.trace_used[cpu]>NESTED_ATOMIC_TRACES)return false;
  log_text("N43_TRACE_OWNER=");log_hex(cpu);
  log_text("N43_TRACE_USED=");log_hex(atomic_runtime.trace_used[cpu]);
  log_text("N43_TRACE_CAPACITY=");log_hex(NESTED_ATOMIC_TRACES);
  if(a->target_promotions[0]!=(cpu?1u:3u)||a->target_promotions[1]!=3||a->target_promotions[2]!=(cpu?3u:0u))return false;
 }
 log_text("N43_TRACE_TOTAL=");log_hex(atomic_runtime.trace_used[0]+atomic_runtime.trace_used[1]);
 if(!retrack_snapshot_data(true)||atomic_runtime.rounds[0].next_stamp.epoch!=2||
    atomic_runtime.rounds[1].next_stamp.epoch!=3)return false;
 if(!ad_updater_gate_close(&atomic_runtime.gate,atomic_runtime.stamp)||
    !ad_updater_gate_drained(&atomic_runtime.gate,atomic_runtime.stamp)||
    !nrc_close(&atomic_runtime.memory.catalog,atomic_runtime.memory.catalog.identity)||
    !nrc_drained(&atomic_runtime.memory.catalog,atomic_runtime.memory.catalog.identity))return false;
 atomic_runtime.finalized=true;return atomic_runtime.receipts==5;
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
