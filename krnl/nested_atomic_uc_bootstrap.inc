/* N46B: one prepublication WB -> UC conversion. Both physical owners remain
 * in VMX root with no guest/VMCS entry until all forty exact receipts commit.
 * Never undo a partial operation: retain every allocation and poison entry. */
#include "nested_atomic_uc_provision.h"
#include "cache_maintenance.h"
static struct {
 struct nauc_controller controller;
 struct nauc_plan plan;
 struct cache_cpu_snapshot snapshots[2];
 struct cache_page_lease host;
 struct nauc_request request;
 struct nauc_receipt response,receipts[40];
 uint64_t last_local[2],broken;
 uint32_t ready[2],sequence,ack,released;
} atomic_uc;
static uint64_t atomic_uc_root(unsigned n){
 const struct nested_domain_memory *d=&domain_memory;
 return (n==0?d->backing.tables[0]:n&1?d->root01[(n-1)/2][0]:d->root02[(n-1)/2][0])|UINT64_C(0x1e);
}
static bool atomic_uc_host_identity(const struct cache_page_lease *p,unsigned type){
 return p->audited&&p->root_hpa==atomic_uc.host.root_hpa&&p->target_hpa==atomic_uc.host.target_hpa&&
  p->table_hpa==atomic_uc.host.table_hpa&&p->index==atomic_uc.host.index&&
  !((p->oldraw^(atomic_uc.host.oldraw|(type==0?UINT64_C(0x18):0)))&~UINT64_C(0x60));
}
static bool atomic_uc_current_guard(void *unused,uint64_t hpa,uint64_t epoch){
 (void)unused;unsigned cpu=cpu_id();struct cache_page_lease current;
 return cpu<2&&epoch==1&&atomic_runtime.memory.type_epoch==epoch&&
  hpa==atomic_runtime.memory.owned[14]&&hpa==atomic_uc.host.target_hpa&&
  __atomic_load_n(&atomic_uc.ready[cpu],__ATOMIC_ACQUIRE)&&
  cache_maintenance_lease_current(&atomic_uc.snapshots[cpu],0,&current)&&atomic_uc_host_identity(&current,0);
}
static bool atomic_uc_capture(unsigned cpu){
 unsigned slot;uint64_t bits;
 uint64_t basic=msr(0x480),primary=msr((basic&(UINT64_C(1)<<55))?0x48e:0x482);
 /* Do not read optional control MSRs before their capability is known. */
 if(!(primary&(UINT64_C(1)<<63)))return false;
 uint64_t secondary=msr(0x48b);if(!(secondary&(UINT64_C(1)<<33)))return false;
 uint64_t ept=msr(0x48c);
 if(cpu_id()!=cpu||!cache_uc_vmx_capable(primary,secondary,ept)||!(ept&(UINT64_C(1)<<21))||
    !cache_maintenance_precheck(atomic_runtime.memory.owned[14],&atomic_uc.snapshots[cpu])||
    !cache_page_pat_slot(atomic_uc.snapshots[cpu].pat,0,&slot,&bits))return false;
 /* precheck requires slot3 UC. Slot selection above validates ALL PAT bytes;
  * it need not return slot3 when a lower existing slot is also UC. */
 __atomic_store_n(&atomic_uc.ready[cpu],1,__ATOMIC_RELEASE);return true;
}
/* No target access or all-WB target audit here: this also runs in the host
 * nonpresent window. Initial/final full inventories are separate phase audits. */
static bool atomic_uc_closed(void){
 unsigned local=cpu_id();
 if(local>1||atomic_failed()||__atomic_load_n(&atomic_uc.released,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&box.request,__ATOMIC_ACQUIRE)||!domain_memory.atomic_catalog_retained||
    !atomic_runtime.memory.uc_staged||atomic_runtime.memory.catalog.published||
    atomic_runtime.memory.catalog.references||atomic_runtime.memory.catalog.sessions||
    atomic_runtime.lease.live_tokens||atomic_runtime.lease.reserved||atomic_runtime.lease.bindings||atomic_runtime.lease.poisoned||
    atomic_runtime.directory.records||atomic_runtime.directory.operations||atomic_runtime.directory.ops||atomic_runtime.directory.poisoned||
    !ad_updater_gate_idle(&atomic_runtime.gate,atomic_runtime.stamp)||
    !bsp_vmx.root_live||!ap_vmx.root_live||bsp_vmx.owner_apic_id!=0||ap_vmx.owner_apic_id!=1||
    smp_vmx_local_check(local?&ap_vmx:&bsp_vmx)!=SMP_VMX_OK)return false;
 for(unsigned cpu=0;cpu<2;cpu++){
  const struct nested_parallel_runtime *r=&nested_runs[cpu];
  if(r->cpu_index!=cpu||r->vcpu.owner_apic!=cpu||r->vcpu.entries||r->vcpu.busy||r->vcpu.poison||
     r->vcpu.launched01||r->vcpu.launched02||r->vcpu.vmcs02||r->vcpu.pending_vmcs||
     r->sparse.initialized||r->sparse.contexts.initialized||r->sparse.contexts.items||r->sparse.contexts.active||r->sparse.contexts.count||
     r->sparse.hardware_item||r->sparse.creations||r->sparse.evictions||r->sparse.selected12||
     r->sparse.pool.live||r->sparse.pool.fault||r->atomic_catalog.used||r->atomic_catalog.entries||
     r->atomic_catalog.atomic.initialized||r->atomic_catalog.ready_entries||
     r->atomic_catalog.atomic.fault.active||r->atomic_catalog.atomic.entry_active)return false;
 }
 return true;
}
static bool atomic_uc_plan_matches(const struct nauc_plan *p){
 const struct nauc_plan *a=&atomic_uc.plan;
 if(p->nonce!=1||p->nonce!=a->nonce||p->type_epoch!=1||p->type_epoch!=atomic_runtime.lease.type_epoch||
    p->type_epoch!=atomic_runtime.memory.catalog.type_epoch||p->target_gpa!=0xf000||
    p->target_gpa!=atomic_runtime.memory.catalog.uc_gpa||p->target_hpa!=atomic_runtime.memory.owned[14]||
    p->target_hpa!=domain_memory.backing.pages[14]||p->target_hpa!=atomic_runtime.memory.catalog.uc_hpa||
    p->host_pte!=atomic_uc.host.table_hpa+8*atomic_uc.host.index||p->host_pte!=a->host_pte||
    p->host_wb_word!=atomic_uc.host.oldraw||p->host_wb_word!=a->host_wb_word||
    p->host_uc_word!=(p->host_wb_word|0x18)||p->host_uc_word!=a->host_uc_word||
    p->host_lease!=atomic_runtime.lease.backing.allocator_lifetime||p->host_lease!=a->host_lease||
    p->inventory!=atomic_runtime.memory.identity.lifetime||p->inventory!=a->inventory||
    p->cpu[0]!=0||p->cpu[1]!=1||a->cpu[0]!=0||a->cpu[1]!=1||
    atomic_runtime.lease.type!=TRL_UC||!atomic_runtime.lease.atomic_uc)return false;
 for(unsigned n=0;n<5;n++)if(p->eptp[n]!=a->eptp[n]||p->eptp[n]!=atomic_uc_root(n))return false;
 return true;
}
static bool atomic_uc_invalidate_fixed(uint64_t eptp){
 struct {uint64_t eptp,reserved;} __attribute__((aligned(16))) descriptor={eptp,0};unsigned char fail;
 __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(descriptor),"r"(UINT64_C(1)):"cc","memory");
 return !fail;
}
static bool atomic_uc_execute_local(const struct nauc_request *q,struct nauc_receipt *out){
 unsigned cpu=cpu_id();uint32_t proofs=NAUC_GATE_CLOSED|NAUC_RETAINED|NAUC_ROOT_OWNER|NAUC_GUEST_NEVER_ENTERED;
 if(!q||!out||cpu>1||q->ordinal<=atomic_uc.last_local[cpu]||q->ordinal>40||
    (q->cpu!=cpu&&(q->cpu!=NAUC_GLOBAL||cpu))||!atomic_uc_plan_matches(&q->plan)||!atomic_uc_closed())return false;
 struct cache_page_lease current;uint64_t *pte;
 switch(q->phase){
 case NAUC_RETAIN:
  if(q->cpu!=NAUC_GLOBAL||!nested_atomic_memory_uc_staged(&atomic_runtime.memory))return false;
  proofs|=NAUC_UNPUBLISHED;break;
 case NAUC_PREFLIGHT:
  if(q->cpu!=cpu||!cache_maintenance_same(&atomic_uc.snapshots[0],&atomic_uc.snapshots[1])||
     !cache_maintenance_lease(&atomic_uc.snapshots[cpu],&current)||!atomic_uc_host_identity(&current,6)||
     !atomic_private_inventory())return false;
  proofs|=NAUC_STAGED|NAUC_PAT_MTRR_WB|NAUC_UNPUBLISHED;break;
 case NAUC_DETACH:
  if(q->cpu!=NAUC_GLOBAL||!nested_atomic_memory_uc_detach(&atomic_runtime.memory))return false;
  break;
 case NAUC_INVEPT_BEFORE:case NAUC_INVEPT_AFTER:{
  bool member=false;
  for(unsigned n=0;n<5;n++)if(q->eptp==atomic_uc_root(n))member=true;
  if(q->cpu!=cpu||!member||!atomic_runtime.memory.uc_detached||
     atomic_runtime.memory.uc_installed!=(q->phase==NAUC_INVEPT_AFTER))return false;
  if(!atomic_uc_invalidate_fixed(q->eptp)||smp_vmx_local_check(cpu?&ap_vmx:&bsp_vmx)!=SMP_VMX_OK)return false;
  break;}
 case NAUC_HOST_BREAK:
  if(q->cpu!=NAUC_GLOBAL||!cache_maintenance_lease(&atomic_uc.snapshots[0],&current)||
     !atomic_uc_host_identity(&current,6))return false;
  pte=paging_phys_to_virt(atomic_uc.host.table_hpa);if(!pte)return false;
  atomic_uc.broken=__atomic_load_n(&pte[atomic_uc.host.index],__ATOMIC_ACQUIRE);
  if(!cache_page_lease_matches(&atomic_uc.host,atomic_uc.broken))return false;
  atomic_uc.broken&=~UINT64_C(1);__atomic_store_n(&pte[atomic_uc.host.index],atomic_uc.broken,__ATOMIC_RELEASE);break;
 case NAUC_INVLPG_BEFORE:case NAUC_INVLPG_AFTER:
  if(q->cpu!=cpu||!cache_maintenance_tlb(&atomic_uc.snapshots[cpu]))return false;
  break;
 case NAUC_WBINVD_BEFORE:case NAUC_WBINVD_AFTER:
  if(q->cpu!=cpu||!cache_maintenance_cache(&atomic_uc.snapshots[cpu]))return false;
  break;
 case NAUC_HOST_MAKE:
  if(q->cpu!=NAUC_GLOBAL||!atomic_uc.broken)return false;
  pte=paging_phys_to_virt(atomic_uc.host.table_hpa);if(!pte)return false;
  if(__atomic_load_n(&pte[atomic_uc.host.index],__ATOMIC_ACQUIRE)!=atomic_uc.broken)return false;
  __atomic_store_n(&pte[atomic_uc.host.index],atomic_uc.broken|UINT64_C(0x19),__ATOMIC_RELEASE);break;
 case NAUC_AUDIT_UC:
  if(q->cpu!=cpu||!atomic_uc_current_guard(NULL,q->plan.target_hpa,q->plan.type_epoch))return false;
  proofs|=NAUC_CURRENT_UC;break;
 case NAUC_INSTALL:
  if(q->cpu!=NAUC_GLOBAL||!nested_atomic_memory_uc_install(&atomic_runtime.memory,atomic_uc_current_guard,NULL))return false;
  proofs|=NAUC_UNPUBLISHED;break;
 case NAUC_AUDIT_FINAL:
  if(q->cpu!=cpu||!atomic_uc_current_guard(NULL,q->plan.target_hpa,q->plan.type_epoch)||!atomic_private_inventory())return false;
  proofs|=NAUC_UNPUBLISHED|NAUC_CURRENT_UC|NAUC_FINAL_SOURCE_HOST_ALIASES;break;
 case NAUC_PUBLISH:
  if(q->cpu!=NAUC_GLOBAL||!nested_atomic_memory_uc_publish(&atomic_runtime.memory))return false;
  break;
 default:return false;
 }
 if((proofs&q->required_proofs)!=q->required_proofs)return false;
 atomic_uc.last_local[cpu]=q->ordinal;
 *out=(struct nauc_receipt){.action=*q,.proofs=proofs,.completed=true};return true;
}
/* One request outstanding, immutable until acquire acknowledgement. AP never
 * reads controller/receipt-array state until released; BSP alone owns it. */
static bool atomic_uc_dispatch(void *unused,const struct nauc_request *q,struct nauc_receipt *r){
 (void)unused;if(cpu_id()!=0)return false;
 bool ok;
 if(q->cpu!=1)ok=atomic_uc_execute_local(q,r);
 else{
  atomic_uc.request=*q;
  __atomic_store_n(&atomic_uc.sequence,(uint32_t)q->ordinal,__ATOMIC_RELEASE);
  uint64_t start=atomic_clock();
  while(__atomic_load_n(&atomic_uc.ack,__ATOMIC_ACQUIRE)!=(uint32_t)q->ordinal){
   if(atomic_failed()||atomic_clock()-start>UINT64_C(30000000000)){atomic_fail();return false;}
   __asm__ volatile("pause");
  }
  *r=atomic_uc.response;ok=r->completed;
 }
 if(!ok){atomic_fail();return false;}
 atomic_uc.receipts[q->ordinal-1]=*r;
 log_text("N46B_PROVISION_ORDINAL=");log_hex(q->ordinal);
 log_text("N46B_PROVISION_PHASE=");log_hex(q->phase);
 log_text("N46B_PROVISION_CPU=");log_hex(q->cpu);
 log_text("N46B_PROVISION_EPTP=");log_hex(q->eptp);
 log_text("N46B_PROVISION_PROOFS=");log_hex(r->proofs);
 return true;
}
static bool atomic_uc_ap_provision(void){
 if(cpu_id()!=1||smp_vmx_local_enter(&ap_vmx)!=SMP_VMX_OK||!atomic_uc_capture(1)){atomic_fail();return false;}
 uint32_t done=0;uint64_t start=atomic_clock();
 for(;;){
  if(atomic_failed()){return false;}
  if(__atomic_load_n(&atomic_uc.released,__ATOMIC_ACQUIRE))
   return atomic_uc.controller.state==NAUC_PUBLISHED&&atomic_uc.controller.completed_actions==40&&atomic_healthy(NULL);
  uint32_t next=__atomic_load_n(&atomic_uc.sequence,__ATOMIC_ACQUIRE);
  if(next!=done){
   struct nauc_request q=atomic_uc.request;struct nauc_receipt r={0};
   if(q.ordinal!=next||q.cpu!=1||!atomic_uc_execute_local(&q,&r)){atomic_fail();return false;}
   atomic_uc.response=r;done=next;__atomic_store_n(&atomic_uc.ack,next,__ATOMIC_RELEASE);start=atomic_clock();
  }else if(atomic_clock()-start>UINT64_C(30000000000)){atomic_fail();return false;}
  __asm__ volatile("pause");
 }
}
static bool atomic_uc_bsp_provision(void){
 if(cpu_id()!=0||!atomic_uc_capture(0)||!atomic_wait(&atomic_uc.ready[1])||
    !atomic_uc_closed()||!cache_maintenance_same(&atomic_uc.snapshots[0],&atomic_uc.snapshots[1])||
    !cache_maintenance_lease(&atomic_uc.snapshots[0],&atomic_uc.host))return false;
 atomic_uc.plan=(struct nauc_plan){.nonce=1,.type_epoch=1,.target_hpa=atomic_runtime.memory.owned[14],.target_gpa=0xf000,
  .host_pte=atomic_uc.host.table_hpa+8*atomic_uc.host.index,.host_wb_word=atomic_uc.host.oldraw,
  .host_uc_word=atomic_uc.host.oldraw|0x18,.host_lease=atomic_runtime.lease.backing.allocator_lifetime,
  .inventory=atomic_runtime.memory.identity.lifetime,.cpu={0,1}};
 for(unsigned n=0;n<5;n++)atomic_uc.plan.eptp[n]=atomic_uc_root(n);
 log_text("N46B_TARGET_HPA=");log_hex(atomic_uc.plan.target_hpa);
 log_text("N46B_HOST_PTE=");log_hex(atomic_uc.plan.host_pte);
 log_text("N46B_HOST_WB_WORD=");log_hex(atomic_uc.plan.host_wb_word);
 log_text("N46B_HOST_UC_WORD=");log_hex(atomic_uc.plan.host_uc_word);
 for(unsigned cpu=0;cpu<2;cpu++){
  log_text("N46B_INITIAL_CPU=");log_hex(atomic_uc.snapshots[cpu].apic);
  log_text("N46B_INITIAL_CR3=");log_hex(atomic_uc.snapshots[cpu].cr3);
  log_text("N46B_INITIAL_PAT=");log_hex(atomic_uc.snapshots[cpu].pat);
 }
 if(!nauc_run(&atomic_uc.controller,&atomic_uc.plan,atomic_uc_dispatch,NULL)||
    atomic_uc.controller.state!=NAUC_PUBLISHED||atomic_uc.controller.completed_actions!=40||
    !nested_atomic_memory_ready(&atomic_runtime.memory)||!atomic_private_inventory()){atomic_fail();return false;}
 log_text("N46B_BOOTSTRAP_COMMITTED receipts=40 fixed_invept=20 host_invlpg=4 host_wbinvd=4 type_epoch=1\n");
 __atomic_store_n(&atomic_uc.released,1,__ATOMIC_RELEASE);return true;
}
static bool atomic_uc_final(void){
 if(atomic_uc.controller.state!=NAUC_PUBLISHED||atomic_uc.controller.completed_actions!=40||
    !atomic_uc.controller.retained||!domain_memory.atomic_catalog_retained||
    !__atomic_load_n(&atomic_uc.released,__ATOMIC_ACQUIRE)||!atomic_healthy(NULL))return false;
 unsigned invept=0,tlb=0,cache=0;
 for(unsigned n=0;n<40;n++){
  const struct nauc_receipt *r=&atomic_uc.receipts[n];
  if(!r->completed||r->action.ordinal!=n+1||!atomic_uc_plan_matches(&r->action.plan))return false;
  if(r->action.phase==NAUC_INVEPT_BEFORE||r->action.phase==NAUC_INVEPT_AFTER)invept++;
  if(r->action.phase==NAUC_INVLPG_BEFORE||r->action.phase==NAUC_INVLPG_AFTER)tlb++;
  if(r->action.phase==NAUC_WBINVD_BEFORE||r->action.phase==NAUC_WBINVD_AFTER)cache++;
 }
 return invept==20&&tlb==4&&cache==4;
}
