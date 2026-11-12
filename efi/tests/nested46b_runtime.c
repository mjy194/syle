/* Host-injected actual N46B bootstrap, real staged backing/controller and real
 * full-tree cache_page audit. No privileged instruction or guest is executed.
 * The Python runner makes a fresh copy and replaces ONLY fixed INVEPT's body.
 * Production runtime context, prepare and private_inventory are extracted
 * verbatim; no bootstrap predicates or algorithm are reimplemented here. */
#define _POSIX_C_SOURCE 200809L
#define main legacy_memory_main
#define paging_phys_to_virt fixture_phys_to_virt
#define paging_page_rw_wb fixture_page_rw_wb
#include "nested41c_memory.c"
#undef main
#undef paging_phys_to_virt
#undef paging_page_rw_wb
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <time.h>
#include "../../krnl/nested_parallel.h"
#include "../../krnl/smp_vmx.h"
#include "../../krnl/cache_maintenance.h"
#include "../../krnl/nested_atomic_multiround.h"
#include "../../krnl/nested_atomic_uc_provision.h"
static struct nested_domain_memory domain_memory;
static struct nested_parallel_runtime nested_runs[2];
static struct smp_vmx_context bsp_vmx,ap_vmx;
static struct {uint32_t request;} box;
#include "runtime_context.h"
#define HOST_ROOT UINT64_C(0x1000000)
#define HOST_PAT UINT64_C(0x0007040600070406)
#define NX UINT64_C(0x8000000000000000)
static _Alignas(4096) uint64_t host_tables[4][512];
static _Thread_local unsigned local_cpu;
static uint64_t current_pat[2],current_cr3[2],cap_ept,cap_primary,cap_secondary;
static unsigned secondary_reads[2],ept_reads[2],precheck_reads[2];
static unsigned invepts[2],tlbs[2],caches[2],case_count,protected_windows;
static uint64_t invept_roots[2][10];
static bool protected_target,ap_ok;
static _Atomic unsigned hardware_failure;
static unsigned cpu_id(void){return local_cpu;}
void *paging_phys_to_virt(uint64_t p){
 if(p>=HOST_ROOT&&p<HOST_ROOT+sizeof host_tables&&!(p&4095))return host_tables[(p-HOST_ROOT)/4096];
 return fixture_phys_to_virt(p);
}
int paging_page_rw_wb(uint64_t p){
 if(p==domain_memory.backing.pages[14])return fixture_page_rw_wb(p)&&
   (__atomic_load_n(&host_tables[3][15],__ATOMIC_ACQUIRE)&0x99)==1;
 return fixture_page_rw_wb(p);
}
static bool atomic_failed(void){return __atomic_load_n(&atomic_runtime.fault,__ATOMIC_ACQUIRE)!=0;}
static void atomic_fail(void){__atomic_store_n(&atomic_runtime.fault,1,__ATOMIC_RELEASE);}
static uint64_t atomic_clock(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
static bool atomic_wait(uint32_t *p){uint64_t start=atomic_clock();while(!__atomic_load_n(p,__ATOMIC_ACQUIRE)){
 if(atomic_failed()||atomic_clock()-start>UINT64_C(5000000000)){atomic_fail();return false;}}
 return !atomic_failed();
}
static bool atomic_uc_current_guard(void *,uint64_t,uint64_t);
static bool atomic_healthy(void *p){(void)p;return !atomic_failed()&&atomic_uc_current_guard(NULL,atomic_runtime.memory.owned[14],1);}
static uint64_t msr(unsigned n){
 switch(n){case 0x480:return UINT64_C(1)<<55;case 0x48e:case 0x482:return cap_primary;
 case 0x48b:++secondary_reads[local_cpu];assert(cap_primary&(UINT64_C(1)<<63));return cap_secondary;
 case 0x48c:++ept_reads[local_cpu];assert(cap_secondary&(UINT64_C(1)<<33));return cap_ept;default:assert(!"Unexpected MSR");return 0;}
}
enum smp_vmx_status smp_vmx_local_enter(struct smp_vmx_context *v){
 assert(local_cpu==1&&v==&ap_vmx);v->root_live=true;return SMP_VMX_OK;
}
enum smp_vmx_status smp_vmx_local_check(struct smp_vmx_context *v){
 return v&&v->owner_apic_id==local_cpu&&v->root_live&&v->last_vmcs==UINT64_MAX?SMP_VMX_OK:SMP_VMX_CURRENT_VMCS;
}
static bool host_read(void *p,uint64_t h,const uint64_t **v){(void)p;*v=paging_phys_to_virt(h);return *v!=NULL;}
static bool snapshot_stable(const struct cache_cpu_snapshot *s){return s&&local_cpu<2&&s->apic==local_cpu&&s->mtrr_wb&&s->cr3==current_cr3[local_cpu]&&s->pat==current_pat[local_cpu];}
bool cache_maintenance_same(const struct cache_cpu_snapshot *a,const struct cache_cpu_snapshot *b){
 return a&&b&&a->mtrr_wb&&b->mtrr_wb&&a->apic!=b->apic&&a->cr3==b->cr3&&a->pat==b->pat&&a->target==b->target;
}
bool cache_maintenance_lease(const struct cache_cpu_snapshot *s,struct cache_page_lease *out){
 return snapshot_stable(s)&&cache_page_audit(s->cr3,s->target,s->pat,128,host_read,NULL,out)==CACHE_PAGE_AUDIT_OK;
}
bool cache_maintenance_lease_current(const struct cache_cpu_snapshot *s,unsigned type,struct cache_page_lease *out){
 return snapshot_stable(s)&&cache_page_audit_current(s->cr3,s->target,s->pat,type,128,host_read,NULL,out)==CACHE_PAGE_AUDIT_OK;
}
bool cache_maintenance_precheck(uint64_t target,struct cache_cpu_snapshot *out){
 ++precheck_reads[local_cpu];
 struct cache_cpu_snapshot s={.cr3=current_cr3[local_cpu],.pat=current_pat[local_cpu],.target=target,.apic=local_cpu,.mtrr_wb=vmx_memory_region_wb(target)};
 struct cache_page_lease l;if(!cache_maintenance_lease(&s,&l))return false;*out=s;return true;
}
bool cache_maintenance_tlb(const struct cache_cpu_snapshot *s){
 ++tlbs[local_cpu];return snapshot_stable(s)&&atomic_load_explicit(&hardware_failure,memory_order_acquire)!=2;
}
bool cache_maintenance_cache(const struct cache_cpu_snapshot *s){
 ++caches[local_cpu];return snapshot_stable(s)&&atomic_load_explicit(&hardware_failure,memory_order_acquire)!=3;
}
static bool mock_fixed_invept(uint64_t root){
 unsigned n=invepts[local_cpu]++;assert(n<10);invept_roots[local_cpu][n]=root;
 return atomic_load_explicit(&hardware_failure,memory_order_acquire)!=1;
}
static void protect_as_pte(void){
 bool absent=!(__atomic_load_n(&host_tables[3][15],__ATOMIC_ACQUIRE)&1);
 if(absent!=protected_target){assert(!mprotect(ram[14],4096,absent?PROT_NONE:PROT_READ|PROT_WRITE));protected_target=absent;if(absent)++protected_windows;}
}
static void log_text(const char *p){(void)p;protect_as_pte();}
static void log_hex(uint64_t n){(void)n;}
#include "runtime_prepare.h"
#include "runtime_inventory.h"
#include "../../krnl/nested_atomic_uc_bootstrap.inc"
static void restore_target(void){if(protected_target){assert(!mprotect(ram[14],4096,PROT_READ|PROT_WRITE));protected_target=false;}}
static void setup(void){
 restore_target();memset(&atomic_runtime,0,sizeof atomic_runtime);memset(&atomic_uc,0,sizeof atomic_uc);
 memset(nested_runs,0,sizeof nested_runs);memset(&bsp_vmx,0,sizeof bsp_vmx);memset(&ap_vmx,0,sizeof ap_vmx);
 memset(&box,0,sizeof box);memset(host_tables,0,sizeof host_tables);
 memset(invepts,0,sizeof invepts);memset(tlbs,0,sizeof tlbs);memset(caches,0,sizeof caches);memset(invept_roots,0,sizeof invept_roots);
 local_cpu=0;protected_windows=0;ap_ok=false;atomic_store(&hardware_failure,0);
 memset(secondary_reads,0,sizeof secondary_reads);memset(ept_reads,0,sizeof ept_reads);memset(precheck_reads,0,sizeof precheck_reads);
 cap_primary=UINT64_C(1)<<63;cap_secondary=UINT64_C(1)<<33;
 current_cr3[0]=current_cr3[1]=HOST_ROOT;current_pat[0]=current_pat[1]=HOST_PAT;
 cap_ept=(UINT64_C(1)<<6)|(UINT64_C(1)<<14)|(UINT64_C(1)<<20)|(UINT64_C(1)<<21)|(UINT64_C(1)<<25);
 for(unsigned i=0;i<3;i++)host_tables[i][0]=(HOST_ROOT+(i+1)*4096)|3;
 host_tables[3][15]=UINT64_C(0xf000)|NX|3;
 base(&domain_memory);assert(atomic_prepare());
 uint64_t private_hpa=UINT64_C(0x2000000);
 bsp_vmx=(struct smp_vmx_context){.region_hpa=private_hpa,.owner_apic_id=0,.last_vmcs=UINT64_MAX,.root_live=true};private_hpa+=4096;
 ap_vmx=(struct smp_vmx_context){.region_hpa=private_hpa,.owner_apic_id=1,.last_vmcs=UINT64_MAX};private_hpa+=4096;
 for(unsigned c=0;c<2;c++){
  struct nested_parallel_runtime *r=&nested_runs[c];r->cpu_index=c;r->vcpu.owner_apic=c;
  r->vcpu.vmcs01=private_hpa;private_hpa+=4096;r->reserved02=private_hpa;private_hpa+=4096;
  for(unsigned i=0;i<NESTED_OWNER_POOL_PAGES;i++){r->sparse.pages[i].hpa=private_hpa;private_hpa+=4096;}
 }
 assert(atomic_private_inventory());ram[14][9]=UINT64_C(0x46bda7a5afe);
}
static void *ap_thread(void *p){(void)p;local_cpu=1;ap_ok=atomic_uc_ap_provision();return NULL;}
enum injection {NONE,BEFORE,AFTER,PRIMITIVE,EARLY_PUBLISH,LATE_PUBLISH,HOST_ALIAS,EPT_ALIAS,STALE_PAT,STALE_CR3,BAD_EPOCH,BAD_HOST_PTE,PRIVATE_ALIAS,PRIVATE_WB,BAD_VMCS,GUEST_ENTERED,BAD_PLAN,WRONG_CPU,REPLAY,STALE_NONCE};
static enum injection inject;
static unsigned at_ordinal,seen_actions,denied_actions;
static nauc_execute_fn production_dispatch;
static void *production_context;
static bool inject_dispatch(void *context,const struct nauc_request *q,struct nauc_receipt *r){
 (void)context;seen_actions++;assert(q->ordinal==seen_actions);
 assert(q->cpu==NAUC_GLOBAL||q->cpu<2);
 assert(!__atomic_load_n(&atomic_uc.released,__ATOMIC_ACQUIRE));
 assert(!atomic_runtime.memory.catalog.published);
 struct nauc_request changed=*q;
 if(q->ordinal==at_ordinal){
  switch(inject){
  case NONE:break;
  case BEFORE:atomic_fail();++denied_actions;return false;
  case AFTER:break;
  case PRIMITIVE:atomic_store(&hardware_failure,q->phase==NAUC_INVEPT_BEFORE||q->phase==NAUC_INVEPT_AFTER?1:q->phase==NAUC_INVLPG_BEFORE||q->phase==NAUC_INVLPG_AFTER?2:3);break;
  case EARLY_PUBLISH:atomic_runtime.memory.catalog.published=true;break;
  case LATE_PUBLISH:atomic_runtime.memory.catalog.fault=true;break;
  case HOST_ALIAS:host_tables[3][80]=host_tables[3][15];break;
  case EPT_ALIAS:((uint64_t *)paging_phys_to_virt(domain_memory.root02[1][3]))[80]=atomic_runtime.memory.owned[14]|0x33;break;
  case STALE_PAT:current_pat[q->cpu==1?1:0]^=UINT64_C(1)<<8;break;
  case STALE_CR3:current_cr3[q->cpu==1?1:0]+=4096;break;
  case BAD_EPOCH:atomic_runtime.memory.type_epoch++;break;
  case BAD_HOST_PTE:host_tables[3][15]^=2;break;
  case PRIVATE_ALIAS:nested_runs[1].reserved02=atomic_runtime.memory.owned[14];break;
  case PRIVATE_WB:denied_wb=nested_runs[1].reserved02;break;
  case BAD_VMCS:(q->cpu==1?&ap_vmx:&bsp_vmx)->last_vmcs=0xdead000;break;
  case GUEST_ENTERED:nested_runs[1].vcpu.entries++;break;
  case BAD_PLAN:changed.plan.inventory++;break;
  case WRONG_CPU:changed.cpu=q->cpu==1?0:1;break;
  case REPLAY:changed.ordinal=atomic_uc.last_local[q->cpu==1?1:0];break;
  case STALE_NONCE:changed.plan.nonce++;break;
  }
 }
 /* Protect before the actual HOST_BREAK body, covering that body as well
  * as every callback until HOST_MAKE has completed. Host audits must inspect
  * only mapping tables, never this data page. */
 if(q->phase==NAUC_HOST_BREAK&&!protected_target){
  assert(!mprotect(ram[14],4096,PROT_NONE));protected_target=true;++protected_windows;
 }
 bool ok=production_dispatch(production_context,&changed,r);
 if(q->ordinal==at_ordinal&&inject==AFTER){atomic_fail();++denied_actions;return false;}
 if(!ok)++denied_actions;
 return ok;
}
bool __real_nauc_run(struct nauc_controller *,const struct nauc_plan *,nauc_execute_fn,void *);
bool __wrap_nauc_run(struct nauc_controller *c,const struct nauc_plan *p,nauc_execute_fn f,void *ctx){
 production_dispatch=f;production_context=ctx;return __real_nauc_run(c,p,inject_dispatch,NULL);
}
static bool run(enum injection how,unsigned at){
 inject=how;at_ordinal=at;seen_actions=denied_actions=0;
 pthread_t ap;assert(!pthread_create(&ap,NULL,ap_thread,NULL));
 bool result=atomic_uc_bsp_provision();if(!result)atomic_fail();assert(!pthread_join(ap,NULL));
 if(result){assert(ap_ok&&atomic_uc_final()&&atomic_uc.controller.state==NAUC_PUBLISHED);assert(protected_windows==1&&!protected_target);assert(seen_actions==40&&!denied_actions);}
 else{assert(!ap_ok&&!atomic_uc_final()&&!atomic_uc.released&&atomic_failed());assert(domain_memory.atomic_catalog_retained);assert(allocated==NAM_OWNED_PAGES);}
 ++case_count;return result;
}
static void successful(void){
 for(unsigned repeat=0;repeat<8;repeat++){
  setup();assert(run(NONE,0));
  assert(atomic_uc.controller.retained&&atomic_uc.controller.completed_actions==40&&nested_atomic_memory_ready(&atomic_runtime.memory));
  assert(ram[14][9]==UINT64_C(0x46bda7a5afe));
  assert(host_tables[3][15]==(UINT64_C(0xf000)|NX|0x1b));
  for(unsigned cpu=0;cpu<2;cpu++){
   assert(invepts[cpu]==10&&tlbs[cpu]==2&&caches[cpu]==2);
   for(unsigned i=0;i<10;i++)assert(invept_roots[cpu][i]==atomic_uc_root(i%5));
  }
  for(unsigned i=0;i<40;i++){
   const struct nauc_receipt *r=&atomic_uc.receipts[i];assert(r->completed&&r->action.ordinal==i+1);
   assert((r->proofs&r->action.required_proofs)==r->action.required_proofs);
  }
  assert(ram[18][15]==0xf043&&ram[26][15]==0x1c073);
  for(unsigned i=0;i<5;i++){
   const uint64_t *roots=i==0?domain_memory.backing.tables:i&1?domain_memory.root01[(i-1)/2]:domain_memory.root02[(i-1)/2];
   assert(((uint64_t *)paging_phys_to_virt(roots[3]))[15]==0xf043);
  }
  struct nrc_session s={0};uint64_t ticket;struct nrc_atomic_page out;
  assert(nrc_begin(&atomic_runtime.memory.catalog,atomic_runtime.memory.roots[0],&atomic_runtime.memory,&s,&ticket));
  assert(nrc_resolve_atomic(&s,ticket,0xf000,1,&out)&&out.backing.memory_type==0&&!out.atomic_view);
  assert(nrc_end(&s,ticket));
 }
 puts("PASS N46B actual BSP/AP mailbox: 8 runs, 40 ordered receipts, 20 fixed-root INVEPT, 4 TLB and 4 cache calls, PROT_NONE window");
}
static void fail_each_action(void){
 for(unsigned after=0;after<2;after++)for(unsigned at=1;at<=40;at++){
  setup();assert(!run(after?AFTER:BEFORE,at));
  assert(atomic_uc.controller.state==NAUC_POISONED&&atomic_uc.controller.completed_actions==at-1);
  assert(atomic_uc.controller.retention_attempted&&atomic_uc.controller.retained==(at>1));
  assert(atomic_runtime.memory.catalog.published==(after&&at==40));
  uint64_t snapshot=atomic_uc.controller.completed_actions;
  assert(!__real_nauc_run(&atomic_uc.controller,&atomic_uc.plan,atomic_uc_dispatch,NULL));
  assert(atomic_uc.controller.completed_actions==snapshot&&!atomic_uc.released);
 }
 puts("PASS N46B all 80 before/after action failures poison and retain without retry/release");
 for(unsigned at=1;at<=40;at++){
  setup();assert(!run(EARLY_PUBLISH,at));assert(atomic_uc.controller.state==NAUC_POISONED&&atomic_uc.controller.completed_actions==at-1);
 }
 setup();assert(!run(LATE_PUBLISH,40));assert(!atomic_runtime.memory.catalog.published&&atomic_uc.controller.completed_actions==39);
 puts("PASS N46B 40 premature publications and final publication denial retain closed entry");
 static const unsigned hardware[]={5,6,7,8,9,10,11,12,13,14,16,17,18,19,21,22,23,24,28,29,30,31,32,33,34,35,36,37};
 for(unsigned i=0;i<sizeof hardware/sizeof hardware[0];i++){
  unsigned at=hardware[i];setup();assert(!run(PRIMITIVE,at));
  assert(atomic_uc.controller.state==NAUC_POISONED&&atomic_uc.controller.completed_actions==at-1&&denied_actions==1);
 }
 puts("PASS N46B each of 28 owner-local INVEPT/TLB/cache primitive failures");
}
static void denied_guards(void){
 static const struct {enum injection how;unsigned at;} cases[]={
  {HOST_ALIAS,2},{HOST_ALIAS,25},{HOST_ALIAS,38},{EPT_ALIAS,2},{EPT_ALIAS,27},{EPT_ALIAS,38},
  {STALE_PAT,2},{STALE_PAT,3},{STALE_PAT,25},{STALE_PAT,26},{STALE_CR3,25},{STALE_CR3,26},
  {BAD_EPOCH,25},{BAD_HOST_PTE,15},{BAD_HOST_PTE,20},{PRIVATE_ALIAS,2},{PRIVATE_ALIAS,3},
  {PRIVATE_ALIAS,38},{PRIVATE_ALIAS,39},{PRIVATE_WB,2},{PRIVATE_WB,38},{BAD_VMCS,5},{BAD_VMCS,10},
  {GUEST_ENTERED,1},{GUEST_ENTERED,20},{BAD_PLAN,1},{BAD_PLAN,3},{WRONG_CPU,5},{WRONG_CPU,10},
  {REPLAY,5},{REPLAY,10},{STALE_NONCE,1},{STALE_NONCE,10}};
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
  setup();assert(!run(cases[i].how,cases[i].at));
  assert(atomic_uc.controller.state==NAUC_POISONED&&atomic_uc.controller.completed_actions==cases[i].at-1);
 }
 puts("PASS N46B real host/source aliases, current PAT/CR3/epoch/PTE, private inventory, VMCS/no-guest, plan/owner/stale/replay denials");
}
static void closed_guards(void){
 /* Each mutation applies before entering VMX mocks, tests production predicate. */
 for(unsigned which=0;which<52;which++){
  setup();ap_vmx.root_live=true;assert(atomic_uc_closed());
  struct nested_parallel_runtime *r=&nested_runs[which%2];
  switch(which){
  case 0:atomic_fail();break;case 1:atomic_uc.released=1;break;case 2:box.request=1;break;
  case 3:domain_memory.atomic_catalog_retained=false;break;case 4:atomic_runtime.memory.uc_staged=false;break;
  case 5:atomic_runtime.memory.catalog.published=true;break;case 6:atomic_runtime.memory.catalog.references=1;break;
  case 7:atomic_runtime.memory.catalog.sessions=(void *)1;break;case 8:atomic_runtime.lease.live_tokens=1;break;
  case 9:atomic_runtime.directory.records=(void *)1;break;case 10:atomic_runtime.directory.operations=1;break;
  case 11:bsp_vmx.root_live=false;break;case 12:ap_vmx.root_live=false;break;case 13:ap_vmx.owner_apic_id=0;break;
  case 14:r->cpu_index^=1;break;case 15:r->vcpu.owner_apic^=1;break;case 16:r->vcpu.entries=1;break;
  case 17:r->vcpu.busy=true;break;case 18:r->vcpu.poison=true;break;case 19:r->vcpu.launched01=true;break;
  case 20:r->vcpu.launched02=true;break;case 21:r->vcpu.vmcs02=0x4000;break;case 22:r->vcpu.pending_vmcs=0x4000;break;
  case 23:r->sparse.initialized=true;break;case 24:r->sparse.contexts.items=(void *)1;break;
  case 25:r->sparse.pool.live=1;break;case 26:r->atomic_catalog.used=1;break;
  case 27:r->atomic_catalog.atomic.fault.active=true;break;case 28:r->atomic_catalog.atomic.entry_active=true;break;
  case 29:r->sparse.contexts.active=(void *)1;break;case 30:r->sparse.contexts.count=1;break;
  case 31:r->sparse.pool.fault=true;break;case 32:r->atomic_catalog.entries=1;break;
  case 33:bsp_vmx.owner_apic_id=1;break;case 34:bsp_vmx.last_vmcs=0x4000;break;
  case 35:atomic_runtime.lease.reserved=1;break;case 36:atomic_runtime.lease.bindings=(void *)1;break;
  case 37:atomic_runtime.lease.poisoned=true;break;case 38:atomic_runtime.directory.ops=(void *)1;break;
  case 39:atomic_runtime.directory.poisoned=true;break;case 40:atomic_runtime.gate.references=1;break;
  case 41:atomic_runtime.gate.tokens=(void *)1;break;case 42:atomic_runtime.gate.closed=true;break;
  case 43:atomic_runtime.gate.fault=1;break;case 44:atomic_runtime.stamp.epoch++;break;
  case 45:r->sparse.contexts.initialized=true;break;case 46:r->sparse.hardware_item=(void *)1;break;
  case 47:r->sparse.creations=1;break;case 48:r->sparse.evictions=1;break;case 49:r->sparse.selected12=1;break;
  case 50:r->atomic_catalog.atomic.initialized=true;break;case 51:r->atomic_catalog.ready_entries=1;break;
  }
  assert(!atomic_uc_closed());++case_count;
 }
 puts("PASS N46B 52 actual prepublication/no-guest gate mutations");
}
static void identity_guards(void){
 setup();assert(run(NONE,0));
 for(unsigned which=0;which<16;which++){
  struct nauc_plan p=atomic_uc.plan;assert(atomic_uc_plan_matches(&p));
  switch(which){
  case 0:p.nonce++;break;case 1:p.type_epoch++;break;case 2:p.target_hpa+=4096;break;
  case 3:p.target_gpa+=4096;break;case 4:p.host_pte+=8;break;case 5:p.host_wb_word^=2;break;
  case 6:p.host_uc_word^=2;break;case 7:p.host_lease++;break;case 8:p.inventory++;break;
  case 14:p.cpu[0]=1;break;case 15:p.cpu[1]=0;break;default:p.eptp[which-9]+=4096;break;
  }
  assert(!atomic_uc_plan_matches(&p));++case_count;
 }
 for(unsigned which=0;which<8;which++){
  assert(atomic_uc_current_guard(NULL,0xf000,1));
  uint64_t hpa=0xf000,epoch=1;
  switch(which){
  case 0:local_cpu=2;break;case 1:epoch=2;break;case 2:hpa+=4096;break;
  case 3:atomic_uc.ready[0]=0;break;case 4:atomic_runtime.memory.type_epoch=2;break;
  case 5:host_tables[3][15]^=2;break;case 6:current_pat[0]^=256;break;case 7:current_cr3[0]+=4096;break;
  }
  assert(!atomic_uc_current_guard(NULL,hpa,epoch));++case_count;
  local_cpu=0;atomic_uc.ready[0]=1;atomic_runtime.memory.type_epoch=1;
  host_tables[3][15]=0xf000|NX|0x1b;current_pat[0]=HOST_PAT;current_cr3[0]=HOST_ROOT;
 }
 for(unsigned which=0;which<7;which++){
  struct cache_page_lease l=atomic_uc.host;l.oldraw|=0x18;
  assert(atomic_uc_host_identity(&l,0));
  switch(which){case 0:l.audited=false;break;case 1:l.root_hpa+=4096;break;case 2:l.target_hpa+=4096;break;
  case 3:l.table_hpa+=4096;break;case 4:l.index++;break;case 5:l.oldraw^=2;break;case 6:l.oldraw^=0x60;break;}
  assert(atomic_uc_host_identity(&l,0)==(which==6));++case_count;
 }
 puts("PASS N46B all 16 bound plan fields, 8 current-host guards and 7 lease identity/A-D mutations");
 static const unsigned ept_bits[]={6,14,20,21,25};
 for(unsigned which=0;which<10;which++){
  setup();
  if(which<5)cap_ept&=~(UINT64_C(1)<<ept_bits[which]);
  else switch(which){case 5:local_cpu=1;break;case 6:current_pat[0]|=UINT64_C(2)<<56;break;
   case 7:current_cr3[0]+=4096;break;case 8:denied_wb=0xf000;break;case 9:current_pat[0]|=UINT64_C(6)<<24;break;}
  assert(!atomic_uc_capture(0)&&!atomic_uc.ready[0]);
  if(which<5)assert(secondary_reads[0]==1&&ept_reads[0]==1&&!precheck_reads[0]);
  ++case_count;
 }
 for(unsigned which=0;which<2;which++){
  setup();if(which)cap_secondary=0;else cap_primary=0;
  assert(!atomic_uc_capture(0)&&!atomic_uc.ready[0]&&!ept_reads[0]&&!precheck_reads[0]);
  assert(secondary_reads[0]==which);++case_count;
 }
 puts("PASS N46B no-secondary/no-EPT/AD0 capability and MSR-read-order, wrong capture CPU, illegal/no-UC PAT, CR3 and MTRR denials");
}
int main(void){successful();fail_each_action();denied_guards();closed_guards();identity_guards();restore_target();printf("N46B_RUNTIME_OK cases=%u hardware_claim=false\n",case_count);return 0;}
