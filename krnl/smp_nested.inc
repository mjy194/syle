#ifdef VMX_NESTED_DOMAIN_KICK_TEST
#include "nested_kick.h"
#endif
/* N32B orchestration. Only BSP allocates/frees or writes the console. */
static struct nested_parallel_runtime nested_runs[2];
static uint64_t nested_shared;
#ifdef VMX_NESTED_DOMAIN_TEST
#include "nested_domain_memory.h"
static struct nested_domain_memory domain_memory;
static struct nested_domain_dispatch domain_dispatch;
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
#include "nested_source_replace_runtime.inc"
static _Noreturn void source_panic(const char *reason){atomic_fail();panic(reason);}
#define panic(s) source_panic(s)
#elif defined(VMX_NESTED_ATOMIC_RETRACK_TEST)
#ifdef VMX_NESTED_ATOMIC_MULTIROUND_TEST
#ifdef VMX_NESTED_ATOMIC_UC_TEST
#include "nested_atomic_uc_runtime.inc"
#elif defined(VMX_NESTED_ATOMIC_GIGAPAGE_TEST)
#include "nested_atomic_gigapage_runtime.inc"
#elif defined(VMX_NESTED_ATOMIC_LARGEPAGE_TEST)
#include "nested_atomic_largepage_runtime.inc"
#else
#include "nested_atomic_multiround_runtime.inc"
#endif
#else
#include "nested_atomic_retrack_runtime.inc"
#endif
static _Noreturn void retrack_panic(const char *reason){atomic_fail();panic(reason);}
/* BSP orchestration failures also stop peer admission before parking. */
#define panic(s) retrack_panic(s)
#else
#include "nested_atomic_runtime.inc"
#endif
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
#include "nested_typed_maintenance.inc"
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
static struct ad_updater_gate domain_ad_gate;
static struct ad_entry_stamp domain_ad_stamp={0x5333,1,0x10000,1,1};
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
#include "nested_ad_maintenance.inc"
#endif
#endif
#if defined(VMX_NESTED_OWNER_CONTEXT_TEST) && !defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST) && !defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
#include "nested_context_runtime.inc"
#endif
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
#include "nested_local_runtime.inc"
#endif
#ifdef VMX_NESTED_RECLAIM_TEST
#include "nested_reclaim_runtime.inc"
#endif
#endif
static uint64_t nested_page(void **view) {
    uint64_t p=pmm_alloc_page_below(UINT64_C(0x100000000));
    if(!p||!vmx_memory_region_wb(p))return 0;
    *view=paging_phys_to_virt(p);
    if(!*view)return 0;
    for(unsigned i=0;i<512;i++)((uint64_t *)*view)[i]=0;
    return p;
}
static bool smp_nested_prepare(uint32_t bsp,uint32_t ap) {
#ifdef VMX_NESTED_DOMAIN_TEST
    if(!nested_domain_memory_prepare(&domain_memory)||!nested_domain_dispatch_init(&domain_dispatch,0x5333,1))return false;
#ifdef VMX_NESTED_RECLAIM_TEST
    if(!nested_domain_memory_prepare_replacement(&domain_memory))return false;
    *(uint64_t *)paging_phys_to_virt(domain_memory.replacement_hpa)=UINT64_C(0x5566556655665566);
#endif
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
    /* Only L1 may write EPT12 PT leaf. L2 mapping remains read-only. */
    for(unsigned cpu=0;cpu<2;cpu++){
        uint64_t *pt=paging_phys_to_virt(domain_memory.root01[cpu][3]);
        if(pt[19]!=(domain_memory.backing.pages[18]|0x31))return false;
        pt[19]|=2;
    }
    *(uint64_t *)paging_phys_to_virt(domain_memory.backing.pages[10])=UINT64_C(0x7788778877887788);
#endif
#if defined(VMX_NESTED_OWNER_CONTEXT_TEST) && !defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST) && !defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    if(!nested_domain_memory_prepare_contexts(&domain_memory)||!nested_domain_memory_prepare_eviction(&domain_memory))return false;
#else
    if(!nested_domain_memory_prepare_contexts(&domain_memory))return false;
#endif
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
    if(!nested_domain_memory_prepare_atomic_ad(&domain_memory)||
       !ad_updater_gate_init(&domain_ad_gate,domain_ad_stamp))return false;
    ((uint64_t *)paging_phys_to_virt(domain_memory.backing.pages[14]))[2]=UINT64_C(0x35ea35ea35ea35ea);
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    if(bsp!=0||ap!=1)return false;
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
    if(!nested_domain_memory_prepare_typed_dynamic(&domain_memory))return false;
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    if(!typed_catalog_prepare())return false;
#endif
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
    if(!nested_domain_memory_prepare_typed_contexts(&domain_memory))return false;
#else
    if(!nested_domain_memory_prepare_typed_uc(&domain_memory))return false;
#endif
#endif
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    if(bsp!=0||ap!=1||!atomic_prepare())return false;
#endif
    nested_shared=domain_memory.backing.pages[9];
#else
    void *shared;
    nested_shared=nested_page(&shared);
#endif
    if(!nested_shared)return false;
    for(unsigned i=0;i<2;i++) {
        struct nested_parallel_runtime *r=&nested_runs[i];
        if(!nested_parallel_init(r,i))return false;
        uint64_t vmcs01=nested_page(&r->vmcs01_view);
        r->reserved02=nested_page(&r->vmcs02_view);
        if(!vmcs01||!r->reserved02||vmcs01==r->reserved02||
           !nested_vcpu_init(&r->vcpu,i?ap:bsp,vmcs01))return false;
        r->vcpu.parallel=r;r->shared_hpa=nested_shared;
#ifdef VMX_NESTED_OWNER_AD_TEST
        r->ad=(struct nested_owner_ad){.gate=&domain_ad_gate,.stamp=domain_ad_stamp,
            .authority=domain_memory.atomic_ad_prepared};
        r->sparse.atomic_ad=&r->ad;
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
        r->ad_checkpoint=ad_checkpoint;
#endif
#endif
#if defined(VMX_NESTED_OWNER_CONTEXT_TEST) && !defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST) && !defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
        r->local_checkpoint=owner_context_checkpoint;
        r->local_invalidated=owner_context_invalidated;
        r->sparse.multiple=true;
#ifdef VMX_NESTED_OWNER_EVICT_TEST
        r->sparse.evict=true;
#endif
#endif
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
        r->local_checkpoint=domain_local_checkpoint;
        r->local_invalidated=domain_local_invalidated;
#endif
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
        for(unsigned n=0;n<NESTED_OWNER_POOL_PAGES;n++){
            void *view;uint64_t page=nested_page(&view);if(!page)return false;
            r->sparse.pages[n]=(struct nested_sparse_page){page,view};
        }
        if(!nested_owner_pool_init(&r->sparse.pool,i?ap:bsp,1,r->sparse.pages,
            r->sparse.slots,NESTED_OWNER_POOL_PAGES,48))return false;
#endif
#ifdef VMX_NESTED_DOMAIN_TEST
        if(!nested_domain_memory_view(&domain_memory,i,&r->vcpu.fixture))return false;
#ifdef VMX_NESTED_RECLAIM_TEST
        r->reclaim_checkpoint=domain_reclaim_checkpoint;
#endif
        r->domain=&domain_dispatch;r->domain_token=domain_dispatch.tokens[i];
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
        if(!atomic_configure(r))return false;
        r->atomic_checkpoint=atomic_checkpoint;
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
        if(!typed_configure(r))return false;
        r->typed_checkpoint=typed_checkpoint;
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
        r->typed_before_entry=typed_context_before_entry;
#else
        r->sparse.typed=&r->typed;
#endif
#endif
        for(unsigned j=0;j<4;j++)r->composed_tables[j]=domain_memory.root02[i][j];
        r->composed_eptp=r->composed_tables[0]|0x1e;
#else
        if(!ept_fixture_prepare(&r->vcpu.fixture))return false;
#endif
        struct ept_fixture *f=&r->vcpu.fixture;
        uint64_t *pt=paging_phys_to_virt(f->tables[3]);
        uint64_t *private_config=paging_phys_to_virt(f->pages[11
#ifdef VMX_NESTED_DOMAIN_TEST
            +i*32
#endif
            ]);
        if(!pt||!private_config)return false;
#if defined(VMX_NESTED_ATOMIC_UC_TEST)
        pt[10]=nested_shared|0x33; /* Still unpublished original WB inventory. */
#elif defined(VMX_NESTED_OWNER_TYPED_TEST) || defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
        pt[10]=nested_shared|0x73; /* Preserve new profile WB/IPAT1 contract. */
#else
        pt[10]=nested_shared|0x33; /* GPAa000 borrowed WB/RW, ownership unchanged */
#endif
        private_config[0x828/8]=i; /* c820 remains L1 exit counter */
    }
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    if(!nested_domain_memory_typed_ready(&domain_memory))return false;
#endif
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    if(!atomic_private_inventory())return false;
#endif
    return true;
}
static bool nested_local_complete(const struct nested_parallel_runtime *r) {
    return r->vcpu.cleared01&&r->vcpu.cleared02&&!r->vcpu.busy&&!r->vcpu.poison;
}
static __attribute__((noreturn)) void smp_nested_ap_stop(struct mailbox *m,unsigned code) {
    nested_parallel_mark_fault(&nested_runs[1],code);
    m->fault.vector=UINT64_MAX-2;m->fault.error=code;
    __atomic_store_n(&m->fault.valid,1,__ATOMIC_RELEASE);
    __atomic_store_n(&m->parked,1,__ATOMIC_RELEASE);
    for(;;)__asm__ volatile("cli;hlt":::"memory");
}
static __attribute__((noreturn)) void smp_nested_ap(struct mailbox *m) {
#ifdef VMX_NESTED_ATOMIC_UC_TEST
    if(!atomic_uc_ap_provision())smp_nested_ap_stop(m,0x46b1);
#endif
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
    typed_ap_preflight();
#endif
    for(;;) {
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
        if(__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)){
            __atomic_store_n(&m->parked,1,__ATOMIC_RELEASE);
            for(;;)__asm__ volatile("cli;hlt":::"memory");
        }
#endif
        uint32_t req=__atomic_load_n(&m->request,__ATOMIC_ACQUIRE);
        if(req==1)break;
        if(req)smp_nested_ap_stop(m,1);
        __asm__ volatile("pause");
    }
    struct nested_parallel_runtime *r=&nested_runs[1];
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    if(!nested_kick_init(m->lapic,m->expected_id))smp_nested_ap_stop(m,7);
    r->lapic_base=m->lapic;
#endif
    #ifdef VMX_NESTED_ATOMIC_UC_TEST
    if(cpu_id()!=m->expected_id||smp_vmx_local_check(&ap_vmx)!=SMP_VMX_OK)
#else
    if(cpu_id()!=m->expected_id||smp_vmx_local_enter(&ap_vmx)!=SMP_VMX_OK)
#endif
        smp_nested_ap_stop(m,2);
    if(!vmx_guest_run_context(&r->vcpu,r->vcpu.vmcs01,msr(0x480)))
        smp_nested_ap_stop(m,3);
    if(!nested_local_complete(r))smp_nested_ap_stop(m,4);
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    if(!typed_owner_retire(r))smp_nested_ap_stop(m,6);
#endif
    if(smp_vmx_local_leave(&ap_vmx)!=SMP_VMX_OK)smp_nested_ap_stop(m,5);
    if(!nested_parallel_finish(r,0,true))smp_nested_ap_stop(m,6);
    __atomic_store_n(&m->ack,1,__ATOMIC_RELEASE);
    __atomic_store_n(&m->parked,1,__ATOMIC_RELEASE);
    for(;;)__asm__ volatile("cli;hlt":::"memory");
}
static void smp_nested_run(void) {
    struct nested_parallel_runtime *b=&nested_runs[0],*a=&nested_runs[1];
    __atomic_store_n(&box.request,1,__ATOMIC_RELEASE);
    /* Start the AP without waiting here. Legacy profiles use a guest barrier;
     * dynamic typed roots instead rendezvous at bounded root checkpoints. */
    if(!vmx_guest_run_context(&b->vcpu,b->vcpu.vmcs01,msr(0x480))){
#ifdef VMX_NESTED_OWNER_TYPED_TEST
        typed_fatal(b); /* Freeze admission; never read the AP's live trace. */
        log_text("SMP.TYPED_BSP_FAILURE_TRACE_BEGIN\n");
        log_text(b->trace); /* Only this CPU writes it; run has returned. */
        log_text("SMP.TYPED_BSP_FAILURE_TRACE_END\n");
#endif
        panic("SMP NESTED BSP RUN FAILED RETAINED");
    }
    if(!nested_local_complete(b))panic("SMP NESTED BSP VMCS RETAINED");
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    if(!typed_owner_retire(b))panic("SMP TYPED BSP RETIRE RETAINED");
#endif
    if(smp_vmx_local_leave(&bsp_vmx)!=SMP_VMX_OK)
        panic("SMP NESTED BSP CLEANUP FAILED RETAINED");
    if(!nested_parallel_finish(b,0,true))panic("SMP NESTED BSP FINISH FAILED RETAINED");
    bool done=false;
    for(unsigned n=0;n<2000;n++) {
        if(nested_parallel_faulted(a)||__atomic_load_n(&box.fault.valid,__ATOMIC_ACQUIRE))
            panic("SMP NESTED AP FAULT RETAINED");
        if(nested_parallel_done(a)&&__atomic_load_n(&box.parked,__ATOMIC_ACQUIRE)){done=true;break;}
        if(!delay(1193))break;
    }
    if(!done||a->retain||b->retain||a->result||b->result||
       ap_vmx.root_live||bsp_vmx.root_live||!nested_local_complete(a)||
       __atomic_load_n(&box.ack,__ATOMIC_ACQUIRE)!=1)
        panic("SMP NESTED COMPLETION UNSAFE RETAINED");
#ifdef VMX_NESTED_DOMAIN_TEST
    if(!nested_domain_dispatch_final(&domain_dispatch))panic("SMP DOMAIN NOT QUIESCENT RETAIN");
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
    if(!atomic_final())panic("SMP N47B FINAL RETAIN");
    log_text("N47B_SOURCE_REPLACEMENT_PASS=1\nN47B_SOURCE_DATA_RETAINED=1\n");
#elif defined(VMX_NESTED_ATOMIC_UC_TEST)
    if(!atomic_uc_final()||!atomic_final())panic("SMP N46B FINAL RETAIN");
    log_text("N46B_RESET_RECEIPTS=8\nN46B_RETRACKED_RO=8\nN46B_NEW_TARGET_PROMOTIONS=8\nN46B_TOTAL_TARGET_PROMOTIONS=13\nN46B_TERMINAL_RECEIPTS=5\nN46B_UNCHANGED_LAUNCH=2\n");
#elif defined(VMX_NESTED_ATOMIC_GIGAPAGE_TEST)
    if(!atomic_final())panic("SMP N45 FINAL RETAIN");
    log_text("N45_RESET_RECEIPTS=8\n");
#elif defined(VMX_NESTED_ATOMIC_LARGEPAGE_TEST)
    if(!atomic_final())panic("SMP N44 FINAL RETAIN");
    log_text("N44_RESET_RECEIPTS=8\n");
#elif defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST)
    if(!atomic_final())panic("SMP N43 FINAL RETAIN");
    log_text("N43_RESET_RECEIPTS=8\nN43_RETRACKED_RO=8\nN43_NEW_TARGET_PROMOTIONS=8\nN43_TOTAL_TARGET_PROMOTIONS=13\nN43_TERMINAL_RECEIPTS=5\nN43_UNCHANGED_LAUNCH=2\n");
#elif defined(VMX_NESTED_ATOMIC_RETRACK_TEST)
    if(!atomic_final())panic("SMP N42B FINAL RETAIN");
    log_text("N42B_RESET_RECEIPTS=4\nN42B_RETRACKED_RO=4\nN42B_NEW_TARGET_PROMOTIONS=4\nN42B_TOTAL_TARGET_PROMOTIONS=9\nN42B_TERMINAL_RECEIPTS=5\nN42B_UNCHANGED_LAUNCH=2\n");
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
    if(!atomic_final())panic("SMP N41C FINAL RETAIN");
    log_text("N41C_SHARED_AB_DISTINCT_C=1\nN41C_READ_RO_PEER_D1=2\nN41C_FIRST_WRITE_PROMOTED=6\nN41C_WARM_REUSE=4\nN41C_BUDGET2_EVICTION=4\nN41C_RECEIPT_SOURCE_RELEASE_BEFORE_FREE=8\n");
#endif
    log_text("SMP.DOMAIN_DATA_GPA=0xf000 HPA=");log_hex(domain_memory.backing.pages[14]);
    for(unsigned cpu=0;cpu<2;cpu++) {
        log_text("SMP.DOMAIN_EPT01=");log_hex(domain_memory.root01[cpu][0]|0x1e);
        log_text("SMP.DOMAIN_EPT02=");log_hex(domain_memory.root02[cpu][0]|0x1e);
    }
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
    /* Both owners are actually done/CLEAR/OFF and AP parked above. No future
     * updater or hardware AD writer can enter; only now drain shared lifetime. */
    if(!ad_updater_gate_close(&domain_ad_gate,domain_ad_stamp)||
       !ad_updater_gate_drained(&domain_ad_gate,domain_ad_stamp))
        panic("DOMAIN AD UPDATERS NOT DRAINED RETAIN");
    for(unsigned l=0;l<3;l++){
        uint64_t *page=paging_phys_to_virt(domain_memory.backing.pages[15+l]);
        if(!(__atomic_load_n(&page[0],__ATOMIC_ACQUIRE)&0x100))panic("DOMAIN AD PREFIX A MISSING");
    }
    uint64_t *adpt=paging_phys_to_virt(domain_memory.backing.pages[18]);
    uint64_t data_ad=__atomic_load_n(&adpt[15],__ATOMIC_ACQUIRE);
    uint64_t *data=paging_phys_to_virt(domain_memory.backing.pages[14]);
    if((data_ad&0x300)!=0x300||data[0]!=UINT64_C(0x3344334433443344)||
       data[1]!=
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
       UINT64_C(0x35cc35cc35cc35cc)||data[2]
#else
       UINT64_C(0x35ad35ad35ad35ad)||data[2]
#endif
       !=UINT64_C(0x35ea35ea35ea35ea)||
       nested_runs[0].ad.target_promotions||nested_runs[1].ad.target_promotions!=
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
       2)
#else
       1)
#endif
        panic("DOMAIN ATOMIC AD MERGE EVIDENCE");
    log_text("SMP.DOMAIN_ATOMIC_LEAF_AD=");log_hex(data_ad);
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
    if(domain_ad_stamp.epoch!=2||ad_maintenance.readonly_leaf!=0xf133||
       domain_dispatch.domain.maintenance_epoch!=2)
        panic("DOMAIN AD RESET EVIDENCE");
    log_text("SMP.DOMAIN_AD_READONLY_LEAF=");log_hex(ad_maintenance.readonly_leaf);
    log_text("SMP.DOMAIN_ATOMIC_TARGET_PROMOTIONS=0,2 epoch=2\n");
#else
    log_text("SMP.DOMAIN_ATOMIC_TARGET_PROMOTIONS=0,1\n");
#endif
    for(unsigned i=0;i<2;i++){
        log_text("SMP.OWNER_AD_READWALKS=");log_hex(nested_runs[i].ad.readwalks);
        log_text("SMP.OWNER_AD_PROMOTIONS=");log_hex(nested_runs[i].ad.promotions);
    }
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    if(!typed_final())panic("SMP TYPED FINAL RETAINED");
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
    log_text("SMP.DYNAMIC_TYPED_FINAL_RECEIPTS=");log_hex(typed_runtime.receipt_count);
    log_text("SMP.DYNAMIC_TYPED_FINAL_EPOCH=");log_hex(typed_runtime.lease.type_epoch);
    log_text("SMP.DYNAMIC_TYPED_FINAL_TOKENS=");log_hex(typed_runtime.lease.live_tokens);
#endif
#endif
#ifdef VMX_NESTED_OWNER_ROOT_SCOPED_TEST
    for(unsigned cpu=0;cpu<2;cpu++){
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
        log_text("SMP.N39B2_BOUND_RECEIPTS=");
#else
        log_text("SMP.N39B1_BOUND_RECEIPTS=");
#endif
        log_hex(typed_runtime.provider_receipts[cpu]);
    }
#endif
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    for(unsigned phase=0;phase<2;phase++){
        const struct tr_overlap *o=&typed_runtime.overlap[phase];
        if(!tr_overlap_valid(o))panic("SMP N39B2 OVERLAP EVIDENCE RETAINED");
        log_text("SMP.N39B2_OVERLAP_PHASE=");log_hex(phase);
        log_text("SMP.N39B2_HELD_CPU=");log_hex(o->hold.cpu);
        log_text("SMP.N39B2_TYPE_EPOCH=");log_hex(o->hold.completed.scope.ticket.type_epoch);
        log_text("SMP.N39B2_HELD_EPTP=");log_hex(o->hold.completed.invalidation.full_eptp);
        log_text("SMP.N39B2_HELD_OPERATION=");log_hex(o->hold.completed.scope.operation_nonce);
        log_text("SMP.N39B2_HELD_RETIREMENT=");log_hex(o->hold.completed.scope.ticket.nonce);
        log_text("SMP.N39B2_HELD_ACK=");log_hex(o->hold.completed.acked);
        log_text("SMP.N39B2_HELD_FLUSH_SERIAL=");log_hex(o->hold.completed.flush_serial);
        log_text("SMP.N39B2_HELD_VERSION=");log_hex(o->hold.version);
        log_text("SMP.N39B2_PEER_CPU=");log_hex(o->peer.cpu);
        log_text("SMP.N39B2_PEER_EPTP=");log_hex(o->peer.retired.completed.invalidation.full_eptp);
        log_text("SMP.N39B2_PEER_OPERATION=");log_hex(o->peer.retired.completed.scope.operation_nonce);
        log_text("SMP.N39B2_PEER_RETIREMENT=");log_hex(o->peer.retired.completed.scope.ticket.nonce);
        log_text("SMP.N39B2_PEER_ACK=");log_hex(o->peer.retired.completed.acked);
        log_text("SMP.N39B2_PEER_FLUSH_SERIAL=");log_hex(o->peer.retired.completed.flush_serial);
        log_text("SMP.N39B2_PEER_COMMIT=");log_hex(o->peer.retired.receipt.commit_version);
        log_text("SMP.N39B2_HELD_COMMIT=");log_hex(o->final.receipt.commit_version);
        log_text("SMP.N39B2_PEER_CREATIONS=");log_hex(o->peer.after.creations-o->peer.before.creations);
        log_text("SMP.N39B2_PEER_EVICTIONS=");log_hex(o->peer.after.evictions-o->peer.before.evictions);
        log_text("SMP.N39B2_PEER_MISSES=");log_hex(o->peer.after.misses-o->peer.before.misses);
        log_text("SMP.N39B2_PEER_SWITCHES=");log_hex(o->peer.after.switches-o->peer.before.switches);
    }
#endif
    uint64_t *shared=paging_phys_to_virt(nested_shared);
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    if(!tr_overlap_valid(&typed_runtime.overlap[0])||!tr_overlap_valid(&typed_runtime.overlap[1]))
        panic("SMP N39B2 COMPLETION OVERLAP RETAINED");
#endif
    for(unsigned i=0;i<2;i++) {
        unsigned slot=i*8;
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
        if(shared[slot]!=(0x3200u+i)||shared[slot+1]!=(0x47b00000u+i)||shared[slot+2]!=(0x3200u+i)||
           shared[slot+3]!=UINT64_C(0x5566556655665566)||
           shared[slot+4]!=(UINT64_C(0x47b1000000000000)+i*0x100)||shared[slot+5]!=8||
           shared[slot+6]!=(i?UINT64_C(0x99aa99aa99aa99aa):0)||
           shared[slot+7]!=(i?UINT64_C(0x47b0000000000102):0))panic("SMP N47B OWNER TRACE RETAIN");
#elif defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST)
        if(shared[slot]!=(0x3200u+i)||shared[slot+1]!=(0x42b00000u+i)||shared[slot+2]!=(0x3200u+i)||
           shared[slot+4]!=(UINT64_C(0x42b2000000000001)+i*0x100)||shared[slot+5]!=13||
           shared[slot+6]!=(i?UINT64_C(0x99aa99aa99aa99aa):0)||
           shared[slot+7]!=(i?UINT64_C(0x42b2000000000102):0))panic("SMP N43 OWNER TRACE RETAIN");
#elif defined( VMX_NESTED_ATOMIC_RETRACK_TEST)
        if(shared[slot]!=(0x3200u+i)||shared[slot+1]!=(0x42b00000u+i)||shared[slot+2]!=(0x3200u+i)||
           shared[slot+4]!=(UINT64_C(0x42b1000000000001)+i*0x100)||shared[slot+5]!=8||
           shared[slot+6]!=(i?UINT64_C(0x99aa99aa99aa99aa):0)||
           shared[slot+7]!=(i?UINT64_C(0x42b1000000000102):0))panic("SMP N42B OWNER TRACE RETAIN");
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
        if(shared[slot]!=(0x3200u+i)||shared[slot+1]!=(0x41c00000u+i)||shared[slot+2]!=(0x3200u+i)||
           shared[slot+4]!=(UINT64_C(0x41cb000000000000)+i)||shared[slot+5]!=UINT64_C(0x41ca000000000001)||
           shared[slot+6]!=UINT64_C(0x99aa99aa99aa99aa)||shared[slot+7]!=(UINT64_C(0x41cc000000000000)+i))
            panic("SMP N41C OWNER TRACE RETAIN");
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
        if(shared[slot]!=(0x3200u+i)||shared[slot+1]!=(0x39b20000u+i)||
           shared[slot+2]!=(0x3200u+i))panic("SMP N39B2 OWNER TRACE FAILED RETAINED");
#else
        if(shared[slot]!=(0x3200u+i)||shared[slot+1]!=(0x38cd0000u+i)||
           shared[slot+2]!=(0x3200u+i))panic("SMP TYPED SERIAL OWNER EVIDENCE FAILED RETAINED");
#endif
#else
        if(shared[slot]!=(0x3200u+i)||shared[slot+1]!=(0x3200u+(i^1))||
           shared[slot+2]!=(0x3200u+i))panic("SMP NESTED OVERLAP EVIDENCE FAILED RETAINED");
#endif
#ifdef VMX_NESTED_DOMAIN_TEST
#ifndef VMX_NESTED_SOURCE_REPLACE_TEST
        if(shared[slot+3]!=UINT64_C(0x3344334433443344))panic("DOMAIN SHARED READ FAILED RETAIN");
#endif /* N47 already proved the replacement sentinel in its owner report. */
        log_text("SMP.DOMAIN_SHARED_READ=");log_hex(shared[slot+3]);
#ifdef VMX_NESTED_OWNER_TYPED_TEST
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
        for(unsigned context=0;context<3;context++){
            uint64_t expected=UINT64_C(0x38cd38cd38cd3800)+i*3+context;
            if(shared[slot+4+context]!=expected)panic("DOMAIN TYPED DYNAMIC UC EVIDENCE");
            log_text("SMP.DOMAIN_TYPED_DYNAMIC_READ=");log_hex(expected);
        }
        if(shared[slot+7]!=UINT64_C(0x38cd38cd38cd3800)+i*3)panic("DOMAIN TYPED DYNAMIC WB EVIDENCE");
        if(nested_runs[i].sparse.creations<6||nested_runs[i].sparse.evictions<4||
           nested_runs[i].sparse.peak_live>NESTED_OWNER_POOL_PAGES)panic("DOMAIN TYPED DYNAMIC EVICTION EVIDENCE");
        log_text("SMP.DOMAIN_TYPED_DYNAMIC_CREATIONS=");log_hex(nested_runs[i].sparse.creations);
        log_text("SMP.DOMAIN_TYPED_DYNAMIC_EVICTIONS=");log_hex(nested_runs[i].sparse.evictions);
        log_text("SMP.DOMAIN_TYPED_DYNAMIC_POOL_PEAK=");log_hex(nested_runs[i].sparse.peak_live);
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
        for(unsigned context=0;context<2;context++){
            uint64_t expected=UINT64_C(0x38bc38bc38bc3800)+i+2*context;
            if(shared[slot+4+context]!=expected||shared[slot+6+context]!=expected)
                panic("DOMAIN TYPED CONTEXT UC/WB READ EVIDENCE");
            log_text("SMP.DOMAIN_TYPED_CONTEXT_READ=");log_hex(expected);
        }
#else
        if(shared[slot+4]!=(UINT64_C(0x36bc36bc36bc3600)+i)||shared[slot+5]!=shared[slot+4])
            panic("DOMAIN TYPED UC/WB READ EVIDENCE");
#endif
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
        log_text("SMP.DOMAIN_WT_READ=");
#else
        log_text("SMP.DOMAIN_UC_READ=");
#endif
        log_hex(shared[slot+4]);
        log_text("SMP.DOMAIN_RESTORED_WB_READ=");
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
        log_hex(shared[slot+7]);
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
        log_hex(shared[slot+6]);
#else
        log_hex(shared[slot+5]);
#endif
#endif
#ifdef VMX_NESTED_RECLAIM_TEST
        if(shared[slot+4]!=UINT64_C(0x5566556655665566)||
           !domain_reclaim.complete||domain_dispatch.domain.acks!=3)
            panic("DOMAIN RECLAIM NEW READ FAILED RETAIN");
        log_text("SMP.DOMAIN_RECLAIM_NEW_READ=");log_hex(shared[slot+4]);
#endif
#endif
        if(nested_runs[i].trace_overflow)panic("SMP NESTED TRACE OVERFLOW RETAINED");
        log_text(i?"SMP.NESTED_AP_TRACE\n":"SMP.NESTED_BSP_TRACE\n");
        log_text(nested_runs[i].trace);
    }
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    if(shared[8+5]!=UINT64_C(0x5566556655665566))panic("DOMAIN KICK RESUME EVIDENCE");
#endif
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
    for(unsigned i=0;i<2;i++){
        struct nested_owner_sparse *s=&nested_runs[i].sparse;
        if(!s->destroyed||s->misses<3||s->pool.live||s->pool.fault
#ifndef VMX_NESTED_SOURCE_REPLACE_TEST
           ||!nested_owner_pool_release_all(&s->pool,s->pool.owner,1,true)
#endif
           )
            panic("DOMAIN SPARSE OWNER RELEASE RETAIN");
        log_text("SMP.OWNER_SPARSE_MISSES=");log_hex(s->misses);
/* N34C2 has one WB A/B/C/A round; N38C-B checks its longer sequence above. */
#if defined(VMX_NESTED_OWNER_EVICT_TEST) && !defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
        if(s->creations!=4||s->evictions!=2||s->peak_live>NESTED_OWNER_POOL_PAGES)
            panic("OWNER EVICTION EVIDENCE");
        log_text("SMP.OWNER_CONTEXT_CREATED=");log_hex(s->creations);
        log_text("SMP.OWNER_CONTEXT_EVICTED=");log_hex(s->evictions);
        log_text("SMP.OWNER_POOL_PEAK=");log_hex(s->peak_live);
#endif
#ifndef VMX_NESTED_SOURCE_REPLACE_TEST
        for(unsigned n=0;n<NESTED_OWNER_POOL_PAGES;n++)
            if(!pmm_free_page(s->pages[n].hpa))panic("OWNER POOL PAGE FREE");
#endif
    }
#endif
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
    for(unsigned i=0;i<2;i++){
        if(shared[i*8+4]!=UINT64_C(0x7788778877887788)||
           nested_runs[i].local_invalidations!=1||
           domain_dispatch.domain.cpus[i].cache_generation!=2)
            panic("LOCAL INVEPT EVIDENCE FAILED");
        log_text("SMP.LOCAL_INVEPT_NEW_READ=");log_hex(shared[i*8+4]);
    }
    log_text("SMP.LOCAL_INVEPT_REMOTE_CACHE_PRESERVED=1\n");
#endif
#if defined(VMX_NESTED_OWNER_CONTEXT_TEST) && !defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST) && !defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
    for(unsigned i=0;i<2;i++)
        if(nested_runs[i].local_invalidations!=2||
           domain_dispatch.domain.cpus[i].cache_generation!=3)
            panic("OWNER CONTEXT LOCAL INVEPT EVIDENCE");
    log_text("SMP.OWNER_CONTEXT_REMOTE_TWO_CACHES_PRESERVED=2\n");
#endif
    /* Both owner CPUs cleared both VMCSs, left VMX and AP is permanently
     * parked. Only now can BSP release original ownership arrays. */
    for(unsigned i=0;i<2;i++) {
        struct nested_parallel_runtime *r=&nested_runs[i];
#ifndef VMX_NESTED_DOMAIN_TEST
        ept_fixture_owned_release(&r->vcpu.fixture);
#endif
#ifndef VMX_NESTED_SOURCE_REPLACE_TEST
        if(!pmm_free_page(r->vcpu.vmcs01)||!pmm_free_page(r->reserved02))
            panic("SMP NESTED VMCS FREE FAILED");
#else
        (void)r; /* N47 retains VMCS/pool backing as well as source and DATA. */
#endif
    }
#ifdef VMX_NESTED_DOMAIN_TEST
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    for(unsigned cpu=0;cpu<2;cpu++){
        if(typed_runtime.owners[cpu].catalog_checks!=15||typed_runtime.owners[cpu].source_session.live)
            panic("SMP N40B CATALOG NEGATIVES RETAINED");
        log_text("SMP.N40B_OWNER_CATALOG_CHECKS=");log_hex(typed_runtime.owners[cpu].catalog_checks);
    }
    if(!nrc_close(&typed_catalog,typed_catalog.identity)||!nrc_drained(&typed_catalog,typed_catalog.identity))
        panic("SMP N40B CATALOG RELEASE RETAINED");
#endif
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    /* N41B atomic leases intentionally grant no source backing release. */
    if(!atomic_runtime.finalized||!domain_memory.ready)panic("N41C SOURCE RETENTION LOST");
#else
    nested_domain_memory_release(&domain_memory);
#endif
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
    log_text("N47B_ALL_BACKING_RETAINED=1\n");
#else
    if(!pmm_free_page(bsp_vmx.region_hpa)||
#endif
#else
    if(!pmm_free_page(nested_shared)||!pmm_free_page(bsp_vmx.region_hpa)||
#endif
#ifndef VMX_NESTED_SOURCE_REPLACE_TEST
       !pmm_free_page(ap_vmx.region_hpa))panic("SMP NESTED OWNED FREE FAILED");
#endif
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
    log_text("SMP.N47B_PASS owners=2 roots=3 WB4K_IPAT1_AD1=1 source_replace=1 checkpoints_per_owner=8 retired_old_roots=4 new_private_firstwrites=2 total_target_promotions=7 terminal_receipts=11 source_data_retained=1 pool_retained=1 launch_preserved=2 type_epoch=1 source_generations=1_2\n");
#elif defined(VMX_NESTED_ATOMIC_UC_TEST)
    log_text("SMP.N46B_PASS owners=2 roots=3 UC4K_IPAT1_AD1=1 immutable_source=1 rounds=2 checkpoints_per_owner=13 source_pages=9 reset_receipts=8 retracked_RO=8 total_target_promotions=13 creations=5 evictions=1 terminal_receipts=5 backing_retained=1 type_epoch=1 ad_epochs=1_2_3 provision_receipts=40 fixed_invept=20 host_invlpg=4 host_wbinvd=4\n");
#elif defined(VMX_NESTED_ATOMIC_GIGAPAGE_TEST)
    log_text("SMP.N45_PASS owners=2 roots=3 immutable_source=1 ept12_1g=1 ept02_4k=1 rounds=2 backing_retained=1\n");
#elif defined(VMX_NESTED_ATOMIC_LARGEPAGE_TEST)
    log_text("SMP.N44_PASS owners=2 roots=3 immutable_source=1 ept12_2m=1 ept02_4k=1 rounds=2 backing_retained=1\n");
#elif defined(VMX_NESTED_ATOMIC_MULTIROUND_TEST)
    log_text("SMP.N43_PASS owners=2 roots=3 WB_IPAT1_AD1=1 immutable_source=1 rounds=2 checkpoints_per_owner=13 source_pages=9 reset_receipts=8 retracked_RO=8 new_target_promotions=8 total_target_promotions=13 creations=5 evictions=1 terminal_receipts=5 source_pins_round=4 launch_preserved=2 source_drained=1 backing_retained=1 epochs=1_2_3\n");
#elif defined(VMX_NESTED_ATOMIC_RETRACK_TEST)
    log_text("SMP.N42B_PASS owners=2 roots=3 WB_IPAT1_AD1=1 immutable_source=1 rounds=1 source_pages=9 reset_receipts=4 retracked_RO=4 new_target_promotions=4 total_target_promotions=9 terminal_receipts=5 source_pins_round=4 launch_preserved=2 source_drained=1 backing_retained=1\n");
#elif defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
    log_text("SMP.N41C_PASS owners=2 roots=3 WB_IPAT1_AD1=1 private02_AD1=2 actual_bound_invept=8 target_first_writes=6 warm_reuse=4 creations=4x2 evictions=2x2 source_drained=1 backing_retained=1 no_clear_cycle=1\n");
#else
#ifdef VMX_NESTED_DOMAIN_TEST
#ifdef VMX_NESTED_RECLAIM_TEST
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    log_text("SMP.N33D_PASS AP_L2_actual_kick=1 BSP_root_coordinator=1 L0_owned_IRQ=1 original_RIP_resume=1 new_backing=2\n");
#else
    log_text("SMP.N33C_PASS stopped_owners=2 historical_targets=3 local_INVEPT01_02=2 new_backing_reads=2 retained_until_clear_off=1\n");
#endif
#else
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
#ifdef VMX_NESTED_LOCAL_INVEPT_TEST
    log_text("SMP.N34B_PASS native_L1_INVEPT=2 local_owner_flush=2 shared_writer=1 remote_software_cache_preserved=1\n");
#else
#if defined(VMX_NESTED_OWNER_CONTEXT_TEST) && !defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST) && !defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    log_text("SMP.N34C2_PASS owner_registries=2 budget=2 roots=3 recreated_A=2 evictions=2x2 type1_type2_local=1\n");
#else
    log_text("SMP.N34C1_PASS owner_registries=2 roots_per_owner=2 native_type1_type2=2x2 reads_ABABA=2 no_eviction=1\n");
#endif
#else
#ifdef VMX_NESTED_OWNER_AD_TEST
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
    log_text("SMP.N35C_PASS owner_reset_INVEPT=2 updater_epoch=2 readonly_A1D0=1 new_write_D1=1 same_VMCS_resume=1 no_backing_free_authority=1\n");
#else
    log_text("SMP.N35B1_PASS immutable_shared_AD1=1 private02_AD1=2 atomic_merge=1 target_firstD_trap=1 updater_drained=1 no_clear_cycle=1\n");
#endif
#else
#ifdef VMX_NESTED_OWNER_TYPED_TEST
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    log_text("SMP.N40B_PASS catalog_roots=3 owned_spans=63 full_key_source_lifetime=1 authoritative_backing=1 foreign_stale_unowned_permission=2x4 whole_walk_permit=1 WB_UC_WB=1 B2_overlap=1 immutable_AD0=1 no_source_replacement=1\n");
#else
    log_text("SMP.N39B2_PASS owner_budget=2 virtual_roots=3 root_scoped_provider=1 actual_bound_invept=1 WB_overlap=1 UC_reverse_overlap=1 new_pin_vmwrite_old_unpin=1 causal_receipts=1 terminal_serial_gate=1 WB_UC_WB=1 restored_before_free=1\n");
#endif
#elif defined(VMX_NESTED_OWNER_ROOT_SCOPED_TEST)
    log_text("SMP.N39B1_PASS owner_budget=2 virtual_roots=3 root_scoped_provider=1 actual_bound_invept=1 receipt_before_free=1 serial_owner_rounds=1 WB_UC_WB=1 restored_before_free=1\n");
#elif defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST)
    log_text("SMP.N38CB_PASS owner_budget=2 virtual_roots=3 runtime_eviction=1 receipt_before_free=1 serial_owner_rounds=1 WB_UC_WB=1 restored_before_free=1\n");
#elif defined(VMX_NESTED_OWNER_TYPED_CONTEXT_TEST)
    log_text("SMP.N38B_PASS owner_contexts=4 no_eviction=1 all_roots_audited=9 stable_tokens=4 WB_UC_WB=1 restored_before_free=1\n");
#elif defined(VMX_NESTED_OWNER_TYPED_WT_TEST)
    log_text("SMP.N37_PASS WB_WT_WB=1 nested_owners=2 all_roots_audited=7 typed_IPAT1=1 existing_PAT_only=1 restored_before_free=1\n");
#else
    log_text("SMP.N36B_PASS WB_UC_WB=1 nested_owners=2 all_roots_audited=7 typed_IPAT1=1 restored_before_free=1\n");
#endif
#else
    log_text("SMP.N34A_PASS private_owner_pools=2 real_lazy_EPT02=2 shared_L2_read=2 repeated_internal_resume=1\n");
#endif
#endif
#endif
#endif
#else
    log_text("SMP.N33B_PASS shared_GPA_domain=1 shared_EPT12=1 private_EPT02=2 actual_dispatch_owner=1 L2_shared_read=2\n");
#endif
#endif
#else
    log_text("SMP.N32B_PASS nested_CPUs=2 L2_mutual_barrier=1 private_VMCS01_VMCS02=2 owner_clear_off=2\n");
#endif
#endif
}

#ifdef VMX_NESTED_ATOMIC_RETRACK_TEST
#undef panic
#endif
