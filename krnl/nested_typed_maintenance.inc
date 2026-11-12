#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
#include "nested_typed_dynamic_maintenance.inc"
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
#include "nested_typed_context_maintenance.inc"
#else
/* N36B bounded fixture orchestration. BSP alone mutates lease metadata. AP
 * executes only epoch/step commands, never writes DTL or invents flush ACKs. */
#include "cache_maintenance.h"
#include "domain_typed_alias_plan.h"
#include "nested_owner_typed.h"
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
#define TYPED_NONWB 4u
#else
#define TYPED_NONWB 0u
#endif
enum typed_command { TYPED_RESET=1,TYPED_INVEPT,TYPED_TLB,TYPED_CACHE,TYPED_REFRESH };
static struct {
    struct domain_typed_lease lease;
    struct dtl_token tokens[2];uint64_t nonce[2];
    struct cache_cpu_snapshot snapshots[2];struct cache_page_lease hostlease;
    struct domain_typed_root_binding roots[7];struct domain_typed_request request;
    struct domain_typed_inventory inventory;struct dtl_inventory input;
    struct domain_typed_alias_plan plan;struct domain_typed_alias_workspace workspace;
    struct dtl_ticket ticket;struct nested_domain_maintenance_ticket domain_ticket;
    uint32_t stopped[3][2],released[3],fault,retired[2];
    uint64_t sequence,command_epoch,ack_sequence;unsigned command;
    unsigned pat_slot[2];uint64_t pat_bits[2];
    uint32_t preflight_ready,preflight_ok;
    bool initialized;
} typed_runtime;
static bool typed_guard(void *opaque){
    struct nested_parallel_runtime *r=opaque;uint64_t cr0;uint32_t a,d;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(0x277));
    return r&&r->cpu_index<2&&!__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)&&
        !(cr0&UINT64_C(0x60000000))&&((uint64_t)a|((uint64_t)d<<32))==typed_runtime.snapshots[r->cpu_index].pat&&
        (typed_runtime.lease.phase==DTL_OPEN||typed_runtime.lease.phase==DTL_STAMP);
}
static void typed_fatal(void *opaque){
    (void)opaque;
    __atomic_store_n(&typed_runtime.fault,1,__ATOMIC_RELEASE);
    nested_domain_dispatch_markfail(&domain_dispatch);
}
static uint64_t typed_clock(void){uint32_t a,d;__asm__ volatile("rdtsc":"=a"(a),"=d"(d));return (uint64_t)a|((uint64_t)d<<32);}
static bool typed_wait32(uint32_t *word){uint64_t start=typed_clock();
    while(!__atomic_load_n(word,__ATOMIC_ACQUIRE)){
        if(__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)||
           __atomic_load_n(&domain_dispatch.fault_latch,__ATOMIC_ACQUIRE)||typed_clock()-start>UINT64_C(30000000000))return false;
        __asm__ volatile("pause");
    }return true;
}
static bool typed_table(void *unused,uint64_t h,const uint64_t **view){
    (void)unused;bool owned=false;
    for(unsigned l=0;l<4;l++){
        if(h==domain_memory.backing.tables[l])owned=true;
        for(unsigned cpu=0;cpu<2;cpu++)if(h==domain_memory.root01[cpu][l]||h==domain_memory.root02[cpu][l])owned=true;
    }
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned p=0;p<NESTED_OWNER_POOL_PAGES;p++)
        if(nested_runs[cpu].sparse.slots[p].live&&h==nested_runs[cpu].sparse.pages[p].hpa)owned=true;
    if(!owned||!h||(h&4095)||!vmx_memory_region_wb(h))return false;
    *view=paging_phys_to_virt(h);return *view!=0;
}
static bool typed_invept(uint64_t eptp){
    struct {uint64_t eptp,zero;} __attribute__((aligned(16))) descriptor={eptp,0};
    unsigned char fail;uint64_t type=1;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(descriptor),"r"(type):"cc","memory");return !fail;
}
static bool typed_local(struct nested_parallel_runtime*r,unsigned command,uint64_t epoch){
    unsigned cpu=r->cpu_index;if(cpu>1||!epoch)return false;
    switch(command){
    case TYPED_RESET:return nested_owner_sparse_reset(&r->sparse);
    case TYPED_INVEPT:
        return typed_invept(typed_runtime.roots[cpu].eptp)&&typed_invept(typed_runtime.roots[2+cpu].eptp);
    case TYPED_TLB:return cache_maintenance_tlb(&typed_runtime.snapshots[cpu]);
    case TYPED_CACHE:return cache_maintenance_cache(&typed_runtime.snapshots[cpu]);
    case TYPED_REFRESH:return nested_owner_typed_refresh(&r->typed,epoch,typed_runtime.nonce[cpu]);
    default:return false;
    }
}
static bool typed_command_both(unsigned cmd,uint64_t epoch){
    if(typed_runtime.sequence==UINT64_MAX)return false;
    typed_runtime.command=cmd;typed_runtime.command_epoch=epoch;
    uint64_t seq=typed_runtime.sequence+1;
    __atomic_store_n(&typed_runtime.sequence,seq,__ATOMIC_RELEASE);
    if(!typed_local(&nested_runs[0],cmd,epoch))return false;
    uint64_t start=typed_clock();
    while(__atomic_load_n(&typed_runtime.ack_sequence,__ATOMIC_ACQUIRE)!=seq){
        if(__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)||typed_clock()-start>UINT64_C(30000000000))return false;
        __asm__ volatile("pause");
    }return true;
}
static bool typed_scan(void){
    enum domain_typed_status status=domain_typed_inventory_audit_type(&typed_runtime.request,TYPED_NONWB,typed_table,0,
        &typed_runtime.workspace.scan,&typed_runtime.inventory);
    nested_parallel_trace_text(&nested_runs[0],"NESTED.TYPED_SCAN_STATUS=");
    nested_parallel_trace_hex(&nested_runs[0],status);
    return status==DOMAIN_TYPED_OK;
}
static bool typed_precheck(unsigned cpu){
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
    return cache_maintenance_precheck_type(domain_memory.backing.pages[14],4,
        &typed_runtime.snapshots[cpu],&typed_runtime.pat_slot[cpu],&typed_runtime.pat_bits[cpu]);
#else
    typed_runtime.pat_slot[cpu]=3;typed_runtime.pat_bits[cpu]=24;
    return cache_maintenance_precheck(domain_memory.backing.pages[14],&typed_runtime.snapshots[cpu]);
#endif
}
static bool typed_host_audit(unsigned type,struct cache_page_lease *out){
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
    return cache_maintenance_lease_current_type(&typed_runtime.snapshots[0],type,
        typed_runtime.pat_slot[0],typed_runtime.pat_bits[0],out);
#else
    return cache_maintenance_lease_current(&typed_runtime.snapshots[0],type,out);
#endif
}
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
/* Before any owner VMX entry or cache transition. Failure retains allocation;
 * the AP parks, and BSP reports BLOCKED rather than pretending a WT run. */
static void typed_ap_preflight(void){
    bool ok=typed_precheck(1);
    __atomic_store_n(&typed_runtime.preflight_ok,ok?1:0,__ATOMIC_RELEASE);
    __atomic_store_n(&typed_runtime.preflight_ready,1,__ATOMIC_RELEASE);
    if(!ok)for(;;)__asm__ volatile("cli;hlt":::"memory");
}
static bool typed_bsp_preflight(void){
    if(!typed_wait32(&typed_runtime.preflight_ready))panic("TYPED AP PREFLIGHT TIMEOUT RETAINED");
    if(!__atomic_load_n(&typed_runtime.preflight_ok,__ATOMIC_ACQUIRE)||!typed_precheck(0)||
       !cache_maintenance_same(&typed_runtime.snapshots[0],&typed_runtime.snapshots[1])||
       typed_runtime.pat_slot[0]!=typed_runtime.pat_slot[1]||typed_runtime.pat_bits[0]!=typed_runtime.pat_bits[1]){
        typed_fatal(0);log_text("SMP.N37_BLOCKED LOCAL_WT_PROOF no_guest_entry no_transition retained\n");return false;
    }
    log_text("SMP.WT_EXISTING_PAT_SLOT=");log_hex(typed_runtime.pat_slot[0]);
    log_text("SMP.WT_HOST_PTE_BITS=");log_hex(typed_runtime.pat_bits[0]);
    log_text("SMP.WT_BOTH_PAT=");log_hex(typed_runtime.snapshots[0].pat);
    log_text("SMP.WT_MTRR_WB_PROVED_CPUS=2\n");return true;
}
#endif
static bool typed_initialize(void){
    if(!domain_memory.typed_uc_prepared||!cache_maintenance_same(&typed_runtime.snapshots[0],&typed_runtime.snapshots[1])||
       typed_runtime.snapshots[0].apic!=0||typed_runtime.snapshots[1].apic!=1||
       !typed_host_audit(6,&typed_runtime.hostlease)||
       typed_runtime.pat_slot[0]!=typed_runtime.pat_slot[1]||typed_runtime.pat_bits[0]!=typed_runtime.pat_bits[1])return false;
    nested_parallel_trace_text(&nested_runs[0],"NESTED.TYPED_HOST_PREFLIGHT_OK\n");
    for(unsigned i=0;i<7;i++){
        uint64_t h=i<2?domain_memory.root01[i][0]:i<4?nested_runs[i-2].sparse.tree.eptp&~UINT64_C(4095):
            i<6?domain_memory.root02[i-4][0]:domain_memory.backing.tables[0];
        typed_runtime.roots[i]=(struct domain_typed_root_binding){i+1,1,h|0x1e,i<4?UINT64_C(1)<<(i&1):0};
    }
    typed_runtime.request=(struct domain_typed_request){typed_runtime.roots,7,48,domain_memory.backing.pages[14],8192};
    if(!typed_scan())return false;
    struct dtl_inventory *in=&typed_runtime.input;
    *in=(struct dtl_inventory){.domain=domain_dispatch.domain.id,.incarnation=domain_dispatch.domain.incarnation,.epoch=1,
        .participants=3,.host={typed_runtime.hostlease.target_hpa,typed_runtime.hostlease.table_hpa,typed_runtime.hostlease.oldraw,typed_runtime.hostlease.index},
        .roots=7,.aliases=typed_runtime.inventory.alias_count,.refills=3,.complete=true,.dedicated_data_wb=true};
    for(unsigned i=0;i<7;i++){
        struct domain_typed_root_inventory *r=&typed_runtime.inventory.roots[i];
        in->root[i]=(struct dtl_root){r->binding.id,r->binding.generation,r->binding.eptp&~UINT64_C(4095),i&1,
            r->binding.historical_mask,r->scan_digest,i<2?0:i<4?1:2,true,r->aliases};
    }
    for(unsigned i=0;i<in->aliases;i++){
        struct domain_typed_alias *a=&typed_runtime.inventory.aliases[i];
        in->alias[i]=(struct dtl_alias){in->root[a->root_index].id,1,a->gpa,a->hpa,a->raw,a->memory_type,a->raw&7,a->ignore_pat,a->permissions};
    }
    in->refill[0]=(struct dtl_refill){1,1,1,0,1}; /* immutable virtual12 structural source, BSP gated */
    in->refill[1]=(struct dtl_refill){2,1,3,0,1};in->refill[2]=(struct dtl_refill){3,1,4,1,1};
    if(!dtl_init_type(&typed_runtime.lease,in,TYPED_NONWB))return false;
    nested_parallel_trace_text(&nested_runs[0],"NESTED.TYPED_LEDGER_INIT_OK\n");
    for(unsigned cpu=0;cpu<2;cpu++){
        if(!dtl_acquire(&typed_runtime.lease,cpu,cpu+2,1,6,true,false,&typed_runtime.tokens[cpu],&typed_runtime.nonce[cpu]))return false;
        nested_runs[cpu].typed.lease=&typed_runtime.lease;nested_runs[cpu].typed.token=&typed_runtime.tokens[cpu];
        nested_runs[cpu].typed.nonce=typed_runtime.nonce[cpu];nested_runs[cpu].typed.epoch=1;
        nested_runs[cpu].typed.root_index=cpu+2;nested_runs[cpu].typed.target_hpa=domain_memory.backing.pages[14];
        nested_runs[cpu].typed.environment_guard=typed_guard;nested_runs[cpu].typed.guard_context=&nested_runs[cpu];
    }
    typed_runtime.initialized=true;
    nested_parallel_trace_text(&nested_runs[0],"NESTED.TYPED_INITIAL_TOKENS_READY\n");return true;
}
static bool typed_root_acks(struct dtl_ticket t){
    for(unsigned r=0;r<7;r++)for(unsigned cpu=0;cpu<2;cpu++)
        if(typed_runtime.roots[r].historical_mask&(UINT64_C(1)<<cpu))
            if(!dtl_root_ack(&typed_runtime.lease,t,r,cpu))return false;
    return true;
}
static bool typed_cache_stage(struct dtl_ticket t,enum dtl_phase phase,unsigned command){
    if(!typed_command_both(command,t.epoch))return false;
    for(unsigned cpu=0;cpu<2;cpu++)if(!dtl_cpu_ack(&typed_runtime.lease,t,phase,cpu))return false;
    return dtl_advance(&typed_runtime.lease,t,phase);
}
static bool typed_transition(unsigned phase){
    unsigned source=phase?TYPED_NONWB:6,destination=phase?6:TYPED_NONWB;
    struct domain_typed_lease *lease=&typed_runtime.lease;
    struct nested_domain_maintenance_ticket *mt=&typed_runtime.domain_ticket;
    if(!typed_runtime.initialized||lease->phase!=DTL_OPEN||lease->type!=source||
       !nested_domain_dispatch_maintenance_begin(&domain_dispatch,NESTED_DOMAIN_TYPED_TRANSITION,mt)||
       domain_dispatch.domain.maintenance_targets!=3)return false;
    /* Only these two paused owners can refill; resetting locally removes all
     * old leaf records and invalidates each actual EPT02 on its owner. */
    if(!typed_command_both(TYPED_RESET,lease->epoch)||!typed_scan()||
       !typed_host_audit(source,&typed_runtime.hostlease))return false;
    const struct dtl_host *initial=&lease->inventory.host;
    if(typed_runtime.hostlease.root_hpa!=typed_runtime.snapshots[0].cr3||
       typed_runtime.hostlease.target_hpa!=initial->target||typed_runtime.hostlease.table_hpa!=initial->table||
       typed_runtime.hostlease.index!=initial->index||
       ((typed_runtime.hostlease.oldraw^initial->oldraw)&~UINT64_C(0xf8)))return false;
    if(!dtl_begin(lease,destination,3,3,3,&typed_runtime.ticket))return false;
    struct dtl_ticket t=typed_runtime.ticket;
    typed_runtime.plan=(struct domain_typed_alias_plan){0};
    if(!domain_typed_alias_prepare_type(&typed_runtime.plan,&typed_runtime.request,source,destination,t.epoch,TYPED_NONWB,
            typed_table,0,&typed_runtime.workspace)||
       !domain_typed_alias_detach(&typed_runtime.plan,t.epoch,&typed_runtime.workspace))return false;
    for(unsigned r=0;r<7;r++)if(!dtl_detach(lease,t,r))return false;
    for(unsigned f=0;f<3;f++)if(!dtl_revoke(lease,t,f))return false;
    if(!typed_command_both(TYPED_INVEPT,t.epoch)||!typed_root_acks(t)||!dtl_advance(lease,t,DTL_DETACH))return false;
    struct cache_page_lease *host=&typed_runtime.hostlease;
    uint64_t *pte=paging_phys_to_virt(host->table_hpa);if(!pte)return false;
    pte+=host->index;
    uint64_t old=__atomic_load_n(pte,__ATOMIC_ACQUIRE);
    if(!cache_page_lease_matches(host,old))return false;
    __atomic_store_n(pte,0,__ATOMIC_RELEASE);
    if(!dtl_advance(lease,t,DTL_BREAK)||!typed_cache_stage(t,DTL_TLB1,TYPED_TLB)||
       !typed_cache_stage(t,DTL_CACHE1,TYPED_CACHE))return false;
    if(__atomic_load_n(pte,__ATOMIC_ACQUIRE)!=0)return false;
    uint64_t made=(old&~UINT64_C(0x98))|(destination==6?0:typed_runtime.pat_bits[0]);
    __atomic_store_n(pte,made,__ATOMIC_RELEASE);
    if(!dtl_advance(lease,t,DTL_MAKE)||!typed_cache_stage(t,DTL_TLB2,TYPED_TLB)||
       !typed_cache_stage(t,DTL_CACHE2,TYPED_CACHE))return false;
    /* No guest/updater may read this immutable structural source while gated. */
    uint64_t *virtualpt=paging_phys_to_virt(domain_memory.backing.pages[18]);
    if(!virtualpt||virtualpt[15]!=(UINT64_C(0xf000)|0x43|((uint64_t)source<<3)))return false;
    virtualpt[15]=UINT64_C(0xf000)|0x43|((uint64_t)destination<<3);
    if(!domain_typed_alias_publish(&typed_runtime.plan,t.epoch,&typed_runtime.workspace))return false;
    for(unsigned r=0;r<7;r++)if(!dtl_publish_root(lease,t,r))return false;
    if(!typed_command_both(TYPED_INVEPT,t.epoch)||!typed_root_acks(t)||!dtl_advance(lease,t,DTL_PUBLISH))return false;
    for(unsigned cpu=0;cpu<2;cpu++)if(!dtl_prepare_token(lease,t,&typed_runtime.tokens[cpu],typed_runtime.nonce[cpu]))return false;
    if(!typed_command_both(TYPED_REFRESH,t.epoch)||!dtl_advance(lease,t,DTL_STAMP))return false;
    for(unsigned cpu=0;cpu<2;cpu++)if(!nested_domain_dispatch_maintenance_ack(&domain_dispatch,cpu,*mt))return false;
    return nested_domain_dispatch_maintenance_publish(&domain_dispatch,*mt)&&
        nested_domain_dispatch_maintenance_complete(&domain_dispatch,*mt);
}
static bool typed_checkpoint(struct nested_parallel_runtime *r,unsigned phase){
    if(!r||r->cpu_index>1||phase>2)return false;
    unsigned cpu=r->cpu_index;
    if(__atomic_load_n(&typed_runtime.released[phase],__ATOMIC_ACQUIRE)||
       (phase==0&&!__atomic_load_n(&typed_runtime.released[2],__ATOMIC_ACQUIRE))||
       (phase==1&&!__atomic_load_n(&typed_runtime.released[0],__ATOMIC_ACQUIRE)))goto fail;
    nested_parallel_trace_text(r,"NESTED.TYPED_CHECKPOINT=");nested_parallel_trace_hex(r,phase);
    if(phase==2&&!typed_precheck(cpu))goto fail;
    if(phase==2)nested_parallel_trace_text(r,"NESTED.TYPED_LOCAL_PREFLIGHT_OK\n");
    __atomic_store_n(&typed_runtime.stopped[phase][cpu],1,__ATOMIC_RELEASE);
    if(cpu){
        uint64_t last=__atomic_load_n(&typed_runtime.ack_sequence,__ATOMIC_ACQUIRE),start=typed_clock();
        while(!__atomic_load_n(&typed_runtime.released[phase],__ATOMIC_ACQUIRE)){
            if(__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)||
               __atomic_load_n(&domain_dispatch.fault_latch,__ATOMIC_ACQUIRE)||typed_clock()-start>UINT64_C(30000000000))goto fail;
            uint64_t seq=__atomic_load_n(&typed_runtime.sequence,__ATOMIC_ACQUIRE);
            if(seq!=last){
                if(seq!=last+1||!typed_local(r,typed_runtime.command,typed_runtime.command_epoch))goto fail;
                last=seq;__atomic_store_n(&typed_runtime.ack_sequence,seq,__ATOMIC_RELEASE);start=typed_clock();
            }
            __asm__ volatile("pause");
        }
        return true;
    }
    if(!typed_wait32(&typed_runtime.stopped[phase][1])||
       !(phase==2?typed_initialize():typed_transition(phase)))goto fail;
    if(!nested_parallel_trace_text(r,"NESTED.TYPED_CHECKPOINT phase=")||
       !nested_parallel_trace_hex(r,phase)||!nested_parallel_trace_text(r," epoch=")||
       !nested_parallel_trace_hex(r,typed_runtime.lease.epoch)||!nested_parallel_trace_text(r," type=")||
       !nested_parallel_trace_hex(r,typed_runtime.lease.type))goto fail;
    __atomic_store_n(&typed_runtime.released[phase],1,__ATOMIC_RELEASE);return true;
fail:
    /* Fatal latch only. Neither CPU writes ledger phase while the peer may
     * still be in a local action. No recovery, reuse or free follows failure. */
    __atomic_store_n(&typed_runtime.fault,1,__ATOMIC_RELEASE);
    /* No shared ledger writes on failure: the peer may still be reading it. */
    nested_domain_dispatch_markfail(&domain_dispatch);return false;
}
/* Called after both physical VMCS references on THIS owner are clear and its
 * sparse_stop succeeded, BEFORE local VMXOFF. Backing pool is still retained. */
static bool typed_owner_retire(struct nested_parallel_runtime *r){
    if(!r||r->cpu_index>1||!typed_runtime.initialized||
       __atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)||!r->sparse.destroyed)return false;
    unsigned cpu=r->cpu_index;
    if(!typed_invept(typed_runtime.roots[cpu].eptp)||
       !typed_invept(typed_runtime.roots[2+cpu].eptp)||
       !typed_invept(typed_runtime.roots[4+cpu].eptp)||
       (!cpu&&!typed_invept(typed_runtime.roots[6].eptp)))return false;
    __atomic_store_n(&typed_runtime.retired[cpu],1,__ATOMIC_RELEASE);return true;
}
/* BSP only AFTER both owners VMXOFF/park. Evidence comes from retire hooks,
 * not an attempted INVEPT after VMXOFF. No target dereference occurs here. */
static bool typed_final(void){
    if(!typed_runtime.initialized||__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)||
       typed_runtime.lease.phase!=DTL_OPEN||typed_runtime.lease.type!=6||
       !__atomic_load_n(&typed_runtime.retired[0],__ATOMIC_ACQUIRE)||
       !__atomic_load_n(&typed_runtime.retired[1],__ATOMIC_ACQUIRE))return false;
    for(unsigned r=0;r<7;r++)if(!dtl_retire_root(&typed_runtime.lease,r,true,true,true))return false;
    for(unsigned cpu=0;cpu<2;cpu++)if(!dtl_release(&typed_runtime.lease,&typed_runtime.tokens[cpu],cpu,
        typed_runtime.nonce[cpu],127,7))return false;
    return dtl_release_ready(&typed_runtime.lease,127,127,3);
}

static bool typed_configure(struct nested_parallel_runtime *r){
    if(!r||r->cpu_index>1||!domain_memory.typed_uc_prepared)return false;
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
    r->typed.allow_wt=true;
#endif
    r->typed.lease=&typed_runtime.lease;r->typed.token=&typed_runtime.tokens[r->cpu_index];
    r->typed.target_hpa=domain_memory.backing.pages[14];r->typed.root_index=r->cpu_index+2;
    r->typed.environment_guard=typed_guard;r->typed.guard_context=r;
    r->typed.fatal=typed_fatal;
    return true;
}

#endif
