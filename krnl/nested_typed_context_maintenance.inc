/* N38B fixed-membership dynamic-directory integration. BSP alone mutates lease metadata. AP
 * executes only epoch/step commands, never writes DTL or invents flush ACKs. */
#include "cache_maintenance.h"
#include "domain_typed_alias_plan.h"
#include "nested_owner_typed.h"
#include "typed_directory_registry.h"
#ifdef VMX_NESTED_OWNER_TYPED_WT_TEST
#define TYPED_NONWB 4u
#else
#define TYPED_NONWB 0u
#endif
enum typed_command { TYPED_RESET=1,TYPED_INVEPT,TYPED_TLB,TYPED_CACHE,TYPED_REFRESH };
static struct {
    struct domain_typed_lease lease;
    struct dtl_token tokens[4];uint64_t nonce[4];
    struct nested_owner_typed contexts[2][2];
    struct typed_directory directory;struct td_registry_bridge bridges[2];
    struct td_registry_slot slots[2][2];unsigned slots_used[2];
    struct td_record fixed_records[5];struct td_identity fixed_ids[5];
    struct td_receipt fixed_receipts[5];
    struct td_identity binding[9];struct td_record *records[9];
    struct td_snapshot directory_snapshot[9];struct td_ticket freeze;
    struct td_operation refill_op[2];uint64_t refill_nonce[2];bool refill_live[2];
    uint64_t lifetime,flush_serial;uint32_t metadata_lock;bool configured;
    struct cache_cpu_snapshot snapshots[2];struct cache_page_lease hostlease;
    struct domain_typed_root_binding roots[9];struct domain_typed_request request;
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
/* All directory/bridge accesses, including end/retain, use this short lock.
 * The operation permit survives unlocked allocation, flush and refill work. */
static bool typed_meta_lock(void){
    for(unsigned n=0;n<1000000;n++){uint32_t zero=0;
        if(__atomic_compare_exchange_n(&typed_runtime.metadata_lock,&zero,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return true;
        __asm__ volatile("pause");
    }typed_fatal(0);return false;
}
static void typed_meta_unlock(void){__atomic_store_n(&typed_runtime.metadata_lock,0,__ATOMIC_RELEASE);}
static bool typed_identity_equal(struct td_identity a,struct td_identity b){
    return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
        a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
static struct td_registry_slot *typed_slot(unsigned cpu,unsigned k){
    if(cpu>1||k>1)return NULL;
    for(unsigned n=0;n<typed_runtime.slots_used[cpu];n++)
        if(typed_runtime.slots[cpu][n].virtual_key==(k?0x1401e:0x1001e))return &typed_runtime.slots[cpu][n];
    return NULL;
}
static bool typed_provider_reserve(void *opaque,struct td_registry_slot **out,uint64_t *life){
    struct nested_parallel_runtime *r=opaque;unsigned cpu=r->cpu_index;
    if(cpu>1||typed_runtime.slots_used[cpu]>=2||typed_runtime.lifetime==UINT64_MAX)return false;
    *out=&typed_runtime.slots[cpu][typed_runtime.slots_used[cpu]++];*life=++typed_runtime.lifetime;return true;
}
static bool typed_provider_receipt(void *opaque,struct td_registry_slot *slot,struct td_retire_ticket ticket,uint64_t *acked,uint64_t *serial){
    struct nested_parallel_runtime *r=opaque;unsigned cpu=r->cpu_index;
    /* Called only by guarded_destroy AFTER THIS owner's actual INVEPT. No
     * owner migration or additional physical consumer is admitted here. */
    if(cpu>1||slot->identity.owner!=cpu||ticket.history&~(UINT64_C(1)<<cpu)||
       typed_runtime.flush_serial==UINT64_MAX)return false;
    *acked=ticket.history;*serial=++typed_runtime.flush_serial;return true;
}
static bool typed_tx_begin(void *opaque,enum nested_context_operation kind){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return false;
    bool ok=td_registry_transactions()->begin(&typed_runtime.bridges[r->cpu_index],kind);
    typed_meta_unlock();return ok;
}
static void typed_tx_end(void *opaque,enum nested_context_operation kind){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return;
    struct td_registry_bridge *b=&typed_runtime.bridges[r->cpu_index];
    td_registry_transactions()->end(b,kind);bool ok=!b->faulted&&!b->directory->poisoned;
    typed_meta_unlock();if(!ok)typed_fatal(r);
}
static bool typed_tx_reserve(void *opaque,uint64_t key,void **token){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return false;
    bool ok=td_registry_transactions()->reserve(&typed_runtime.bridges[r->cpu_index],key,token);
    typed_meta_unlock();return ok;
}
static bool typed_tx_cancel(void *opaque,void *token){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return false;
    bool ok=td_registry_transactions()->cancel(&typed_runtime.bridges[r->cpu_index],token);
    typed_meta_unlock();return ok;
}
static bool typed_tx_attach(void *opaque,void *token,struct nested_ept_context_item *item,bool initialized){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return false;
    bool ok=td_registry_transactions()->attach(&typed_runtime.bridges[r->cpu_index],token,item,initialized);
    typed_meta_unlock();return ok;
}
static void typed_tx_retain(void *opaque,void *token,struct nested_ept_context_item *item){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return;
    td_registry_transactions()->retain(&typed_runtime.bridges[r->cpu_index],token,item);
    typed_meta_unlock();typed_fatal(r);
}
static bool typed_tx_publish(void *opaque,void *token,struct nested_ept_context_item *item){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return false;
    struct td_registry_bridge *b=&typed_runtime.bridges[r->cpu_index];struct td_registry_slot *slot=token;
    bool ok=td_registry_transactions()->publish(b,token,item)&&
        td_refill_add(b->directory,&b->operation,b->nonce,&slot->record,slot->identity);
    typed_meta_unlock();return ok;
}
static bool typed_tx_reset(void *opaque,void *token,struct nested_ept_context_item *item){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return false;
    struct td_registry_bridge *b=&typed_runtime.bridges[r->cpu_index];struct td_registry_slot *slot=token;
    bool ok=td_registry_transactions()->reset(b,token,item)&&slot->record.refills==1&&
        td_refill_drop(b->directory,&b->operation,b->nonce,&slot->record,slot->identity);
    typed_meta_unlock();return ok;
}
static bool typed_tx_retire(void *opaque,void *token,struct nested_ept_context_item *item){
    struct nested_parallel_runtime *r=opaque;if(!typed_meta_lock())return false;
    struct td_registry_bridge *b=&typed_runtime.bridges[r->cpu_index];struct td_registry_slot *slot=token;
    bool ok=slot->record.refills<=1;
    if(ok&&slot->record.refills)ok=td_refill_drop(b->directory,&b->operation,b->nonce,&slot->record,slot->identity);
    if(ok)ok=td_registry_transactions()->retire(b,token,item);
    typed_meta_unlock();return ok;
}
static const struct nested_context_transactions typed_transactions={typed_tx_begin,typed_tx_end,typed_tx_reserve,
    typed_tx_cancel,typed_tx_attach,typed_tx_retain,typed_tx_publish,typed_tx_reset,typed_tx_retire};
static bool typed_refill_begin(struct nested_owner_sparse *s){
    unsigned cpu=s->pool.owner;if(cpu>1||!typed_runtime.initialized||!typed_meta_lock())return false;
    struct td_registry_slot *slot=s->contexts.active?s->contexts.active->transaction_token:NULL;
    bool ok=!typed_runtime.refill_live[cpu]&&slot&&slot->record.state==TD_LIVE&&slot->record.refills==1&&
        slot->record.cache_generation==s->contexts.active->tree.generation&&
        !typed_runtime.bridges[cpu].faulted&&!__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)&&
        td_begin(&typed_runtime.directory,&typed_runtime.refill_op[cpu],&typed_runtime.refill_nonce[cpu]);
    if(ok)typed_runtime.refill_live[cpu]=true;
    typed_meta_unlock();return ok;
}
static bool typed_refill_end(struct nested_owner_sparse *s){
    unsigned cpu=s->pool.owner;if(cpu>1||!typed_meta_lock())return false;
    bool ok=typed_runtime.refill_live[cpu]&&td_end(&typed_runtime.directory,&typed_runtime.refill_op[cpu],typed_runtime.refill_nonce[cpu]);
    if(ok)typed_runtime.refill_live[cpu]=false;
    typed_meta_unlock();if(!ok)typed_fatal(0);return ok;
}
static bool typed_context_before_entry(struct nested_parallel_runtime *r,bool in_l2){
    if(!r||r->cpu_index>1||!typed_runtime.initialized||!typed_meta_lock())return false;
    unsigned cpu=r->cpu_index;struct td_record *record=typed_runtime.records[cpu];struct td_identity id=typed_runtime.binding[cpu];
    bool ok=!typed_runtime.bridges[cpu].faulted&&!__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE);
    if(in_l2){
        struct nested_ept_context_item *item=r->sparse.hardware_item;
        struct td_registry_slot *slot=item?item->transaction_token:NULL;
        ok=ok&&item&&item==r->sparse.contexts.active&&item->pins&&slot&&slot->item==item&&!slot->forgotten;
        if(ok){record=&slot->record;id=slot->identity;}
    }
    struct td_operation op={0};uint64_t n=0;
    if(ok){ok=td_begin(&typed_runtime.directory,&op,&n);if(ok){bool noted=td_resident(&typed_runtime.directory,&op,n,record,id,cpu);ok=td_end(&typed_runtime.directory,&op,n)&&noted;}}
    typed_meta_unlock();if(!ok)typed_fatal(r);return ok;
}
/* Membership is fixed after bootstrap. Resolve every snapshot entry by full
 * lifetime identity, never by snapshot enumeration index or numerical HPA. */
static bool typed_directory_seal(bool initialize){
    if(!typed_meta_lock())return false;
    bool ok=td_close(&typed_runtime.directory)&&td_seal(&typed_runtime.directory,&typed_runtime.freeze);
    size_t count=0;
    if(ok)ok=td_snapshot_read(&typed_runtime.directory,typed_runtime.freeze,typed_runtime.directory_snapshot,9,&count)&&count==9;
    bool seen[9]={0};
    for(size_t n=0;ok&&n<count;n++){
        struct td_snapshot *s=&typed_runtime.directory_snapshot[n];unsigned i;
        for(i=0;i<9;i++)if(typed_identity_equal(s->identity,typed_runtime.binding[i]))break;
        if(i==9||seen[i]||s->receipt||s->state!=TD_LIVE||!s->attached||
           s->hpa!=(typed_runtime.roots[i].eptp&~UINT64_C(4095))||s->full_eptp!=typed_runtime.roots[i].eptp){ok=false;break;}
        uint64_t expected_refills=initialize?(i==0?2:(i>=2&&i<6?1:0)):0;
        if(s->refills!=expected_refills){ok=false;break;}
        if(i>=2&&i<6){unsigned cpu=(i-2)/2,k=(i-2)%2;struct td_registry_slot *slot=typed_slot(cpu,k);
            if(!slot||!slot->item||s->cache_generation!=slot->item->tree.generation||s->pins!=slot->item->pins){ok=false;break;}}
        else if(s->cache_generation!=1||s->pins){ok=false;break;}
        seen[i]=true;typed_runtime.roots[i].historical_mask=s->history;
        if(!initialize&&!dtl_root_residency_extend(&typed_runtime.lease,i,s->identity.root_id,s->identity.root_incarnation,s->history)){ok=false;break;}
    }
    typed_meta_unlock();return ok;
}
static bool typed_refills_reopen(uint64_t epoch){
    if(!typed_meta_lock())return false;
    bool ok=td_reopen(&typed_runtime.directory,typed_runtime.freeze,epoch);
    struct td_operation op={0};uint64_t n=0;
    if(ok)ok=td_begin(&typed_runtime.directory,&op,&n);
    if(ok){
        for(unsigned i=2;i<6&&ok;i++)ok=typed_runtime.records[i]->refills==0&&
            td_refill_add(&typed_runtime.directory,&op,n,typed_runtime.records[i],typed_runtime.binding[i]);
        for(unsigned k=0;k<2&&ok;k++)ok=td_refill_add(&typed_runtime.directory,&op,n,typed_runtime.records[0],typed_runtime.binding[0]);
        bool ended=td_end(&typed_runtime.directory,&op,n);ok=ok&&ended;
    }typed_meta_unlock();return ok;
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
        for(unsigned i=0;i<9;i++)if(typed_runtime.roots[i].historical_mask&(UINT64_C(1)<<cpu))
            if(!typed_invept(typed_runtime.roots[i].eptp))return false;
        return true;
    case TYPED_TLB:return cache_maintenance_tlb(&typed_runtime.snapshots[cpu]);
    case TYPED_CACHE:return cache_maintenance_cache(&typed_runtime.snapshots[cpu]);
    case TYPED_REFRESH:
        for(unsigned k=0;k<2;k++)if(!nested_owner_typed_refresh(&typed_runtime.contexts[cpu][k],epoch,typed_runtime.nonce[cpu*2+k]))return false;
        return true;
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
    if(!domain_memory.typed_uc_prepared||!domain_memory.typed_contexts_prepared||!cache_maintenance_same(&typed_runtime.snapshots[0],&typed_runtime.snapshots[1])||
       typed_runtime.snapshots[0].apic!=0||typed_runtime.snapshots[1].apic!=1||
       !typed_host_audit(6,&typed_runtime.hostlease)||typed_runtime.directory.type_epoch!=1)return false;
    if(!typed_meta_lock())return false;
    struct td_operation op={0};uint64_t n=0;bool ok=td_begin(&typed_runtime.directory,&op,&n);
    unsigned fixed=0;
    for(unsigned i=0;i<9&&ok;i++){
        if(i>=2&&i<6){unsigned cpu=(i-2)/2,k=(i-2)%2;struct td_registry_slot *slot=typed_slot(cpu,k);
            if(!slot||!slot->record.attached||slot->record.state!=TD_LIVE||!slot->item){ok=false;break;}
            typed_runtime.records[i]=&slot->record;typed_runtime.binding[i]=slot->identity;
        }else{
            uint64_t h=i<2?domain_memory.root01[i][0]:i<8?domain_memory.root02[i-6][0]:domain_memory.backing.tables[0];
            unsigned owner=i<2?i:i<8?i-6:0;
            if(typed_runtime.lifetime==UINT64_MAX){ok=false;break;}
            struct td_record *record=&typed_runtime.fixed_records[fixed];struct td_identity *id=&typed_runtime.fixed_ids[fixed++];
            ok=td_reserve(&typed_runtime.directory,&op,n,record,owner,++typed_runtime.lifetime,id)&&
                td_attach(&typed_runtime.directory,&op,n,record,*id,h|0x1e,h,1,0)&&
                td_publish(&typed_runtime.directory,&op,n,record,*id);
            typed_runtime.records[i]=record;typed_runtime.binding[i]=*id;
        }
        struct td_record *record=typed_runtime.records[i];struct td_identity id=typed_runtime.binding[i];
        typed_runtime.roots[i]=(struct domain_typed_root_binding){id.root_id,id.root_incarnation,record->full_eptp,record->history};
    }
    if(ok)ok=td_refill_add(&typed_runtime.directory,&op,n,typed_runtime.records[0],typed_runtime.binding[0])&&
        td_refill_add(&typed_runtime.directory,&op,n,typed_runtime.records[0],typed_runtime.binding[0]);
    if(op.live){bool ended=td_end(&typed_runtime.directory,&op,n);ok=ok&&ended;}
    typed_meta_unlock();if(!ok)return false;
    typed_runtime.request=(struct domain_typed_request){typed_runtime.roots,9,48,domain_memory.backing.pages[14],8192};
    if(!typed_directory_seal(true)||!typed_scan())return false;
    struct dtl_inventory *in=&typed_runtime.input;
    /* Baseline authorization epoch advances once; this does not claim a
     * cache-type transition. Initial host and every target alias remain WB. */
    *in=(struct dtl_inventory){.domain=domain_dispatch.domain.id,.incarnation=domain_dispatch.domain.incarnation,.epoch=2,
        .participants=3,.host={typed_runtime.hostlease.target_hpa,typed_runtime.hostlease.table_hpa,typed_runtime.hostlease.oldraw,typed_runtime.hostlease.index},
        .roots=9,.aliases=typed_runtime.inventory.alias_count,.refills=6,.complete=true,.dedicated_data_wb=true};
    for(unsigned i=0;i<9;i++){
        struct domain_typed_root_inventory *root=&typed_runtime.inventory.roots[i];
        in->root[i]=(struct dtl_root){root->binding.id,root->binding.generation,root->binding.eptp&~UINT64_C(4095),
            typed_runtime.binding[i].owner,root->binding.historical_mask,root->scan_digest,i<2?0:i<6?1:2,true,root->aliases};
    }
    for(unsigned i=0;i<in->aliases;i++){
        struct domain_typed_alias *a=&typed_runtime.inventory.aliases[i];
        in->alias[i]=(struct dtl_alias){in->root[a->root_index].id,in->root[a->root_index].generation,a->gpa,a->hpa,a->raw,a->memory_type,a->raw&7,a->ignore_pat,a->permissions};
    }
    for(unsigned k=0;k<2;k++)in->refill[k]=(struct dtl_refill){k+1,1,in->root[0].id,0,in->root[0].generation};
    for(unsigned i=0;i<4;i++)in->refill[i+2]=(struct dtl_refill){i+3,1,in->root[i+2].id,typed_runtime.binding[i+2].owner,in->root[i+2].generation};
    if(!dtl_init_type(&typed_runtime.lease,in,0))return false;
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned k=0;k<2;k++){
        unsigned q=cpu*2+k,root=q+2;struct nested_owner_typed *state=&typed_runtime.contexts[cpu][k];
        if(!dtl_acquire(&typed_runtime.lease,cpu,root,2,6,true,false,&typed_runtime.tokens[q],&typed_runtime.nonce[q]))return false;
        state->nonce=typed_runtime.nonce[q];state->epoch=2;state->root_index=root;
    }
    if(!typed_meta_lock())return false;
    ok=td_reopen(&typed_runtime.directory,typed_runtime.freeze,2);
    typed_meta_unlock();if(!ok)return false;
    typed_runtime.initialized=true;
    nested_parallel_trace_text(&nested_runs[0],"NESTED.N38B_DIRECTORY_READY roots=9 contexts=4 baseline_epoch=2\n");return true;
}
static bool typed_root_acks(struct dtl_ticket t){
    for(unsigned r=0;r<9;r++)for(unsigned cpu=0;cpu<2;cpu++)
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
    if(!typed_command_both(TYPED_RESET,lease->epoch)||!typed_meta_lock())return false;
    struct td_operation revoke={0};uint64_t rn=0;
    bool revoked=td_begin(&typed_runtime.directory,&revoke,&rn);
    if(revoked){
        for(unsigned k=0;k<2&&revoked;k++)revoked=td_refill_drop(&typed_runtime.directory,&revoke,rn,typed_runtime.records[0],typed_runtime.binding[0]);
        bool ended=td_end(&typed_runtime.directory,&revoke,rn);revoked=revoked&&ended;
    }
    typed_meta_unlock();
    if(!revoked||!typed_directory_seal(false)||!typed_scan()||
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
    for(unsigned r=0;r<9;r++)if(!dtl_detach(lease,t,r))return false;
    for(unsigned f=0;f<6;f++)if(!dtl_revoke(lease,t,f))return false;
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
    for(unsigned k=0;k<2;k++){
        uint64_t *virtualpt=paging_phys_to_virt(domain_memory.backing.pages[k?22:18]);
        if(!virtualpt||virtualpt[15]!=(UINT64_C(0xf000)|0x43|((uint64_t)source<<3)))return false;
        virtualpt[15]=UINT64_C(0xf000)|0x43|((uint64_t)destination<<3);
    }
    if(!domain_typed_alias_publish(&typed_runtime.plan,t.epoch,&typed_runtime.workspace))return false;
    for(unsigned r=0;r<9;r++)if(!dtl_publish_root(lease,t,r))return false;
    if(!typed_command_both(TYPED_INVEPT,t.epoch)||!typed_root_acks(t)||!dtl_advance(lease,t,DTL_PUBLISH))return false;
    for(unsigned q=0;q<4;q++)if(!dtl_prepare_token(lease,t,&typed_runtime.tokens[q],typed_runtime.nonce[q]))return false;
    if(!typed_command_both(TYPED_REFRESH,t.epoch)||!typed_refills_reopen(t.epoch)||!dtl_advance(lease,t,DTL_STAMP))return false;
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
    /* Context receipts already committed before sparse_stop freed their
     * nodes to this private pool. No allocation/reuse occurs afterward; pool
     * pages remain owned until both owners OFF/park and typed_final. */
    for(unsigned i=0;i<9;i++)if(typed_runtime.binding[i].owner==cpu)
        if(!typed_invept(typed_runtime.roots[i].eptp))return false;
    __atomic_store_n(&typed_runtime.retired[cpu],1,__ATOMIC_RELEASE);return true;
}
/* Owner retire receipts were recorded before VMXOFF. No hardware instruction
 * is attempted by this final BSP bookkeeping after both CPUs have stopped. */
static bool typed_final(void){
    if(!typed_runtime.initialized||__atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)||
       typed_runtime.lease.phase!=DTL_OPEN||typed_runtime.lease.type!=6||
       !__atomic_load_n(&typed_runtime.retired[0],__ATOMIC_ACQUIRE)||
       !__atomic_load_n(&typed_runtime.retired[1],__ATOMIC_ACQUIRE)||!typed_meta_lock())return false;
    struct td_operation op={0};uint64_t n=0;bool ok=td_begin(&typed_runtime.directory,&op,&n);
    if(ok){
        for(unsigned k=0;k<2&&ok;k++)ok=td_refill_drop(&typed_runtime.directory,&op,n,typed_runtime.records[0],typed_runtime.binding[0]);
        unsigned fixed=0;
        for(unsigned i=0;i<9&&ok;i++)if(i<2||i>=6){
            struct td_record *record=typed_runtime.records[i];struct td_retire_ticket ticket;
            if(typed_runtime.flush_serial==UINT64_MAX){ok=false;break;}
            ok=td_retire_begin(&typed_runtime.directory,&op,n,record,typed_runtime.binding[i],&ticket)&&
                td_retire_commit(&typed_runtime.directory,&op,n,record,typed_runtime.binding[i],ticket,record->history,
                    ++typed_runtime.flush_serial,&typed_runtime.fixed_receipts[fixed++])&&
                td_forget(&typed_runtime.directory,&op,n,record,typed_runtime.binding[i]);
        }
        bool ended=td_end(&typed_runtime.directory,&op,n);ok=ok&&ended;
    }
    if(ok)ok=!typed_runtime.directory.records&&!typed_runtime.directory.operations&&!typed_runtime.directory.poisoned&&
        !typed_runtime.bridges[0].faulted&&!typed_runtime.bridges[1].faulted;
    typed_meta_unlock();if(!ok)return false;
    for(unsigned i=0;i<9;i++)if(!dtl_retire_root(&typed_runtime.lease,i,true,true,true))return false;
    for(unsigned q=0;q<4;q++)if(!dtl_release(&typed_runtime.lease,&typed_runtime.tokens[q],q/2,
        typed_runtime.nonce[q],511,63))return false;
    return dtl_release_ready(&typed_runtime.lease,511,511,3);
}
static bool typed_contexts_ready(struct nested_owner_sparse *s){
    unsigned cpu=s->pool.owner;if(cpu>1||s->contexts.count!=2||s->evict||!typed_meta_lock())return false;
    bool ok=!typed_runtime.bridges[cpu].faulted&&typed_runtime.slots_used[cpu]==2;
    for(unsigned k=0;k<2&&ok;k++){
        struct td_registry_slot *slot=typed_slot(cpu,k);
        ok=slot&&slot->record.state==TD_LIVE&&slot->item&&slot->item->tree.generation==1&&
            typed_runtime.contexts[cpu][k].tree==&slot->item->tree;
    }
    typed_meta_unlock();return ok;
}
static bool typed_configure(struct nested_parallel_runtime *r){
    if(!r||r->cpu_index>1||!domain_memory.typed_uc_prepared||!domain_memory.typed_contexts_prepared)return false;
    unsigned cpu=r->cpu_index;
    if(!typed_runtime.configured){
        if(cpu||!td_init(&typed_runtime.directory,domain_dispatch.domain.id,domain_dispatch.domain.incarnation,3,1))return false;
        typed_runtime.configured=true;
    }
    const struct td_registry_provider provider={typed_provider_reserve,typed_provider_receipt};
    if(!td_registry_bridge_init(&typed_runtime.bridges[cpu],&typed_runtime.directory,cpu,&provider,r))return false;
    for(unsigned k=0;k<2;k++){
        struct nested_owner_typed *state=&typed_runtime.contexts[cpu][k];unsigned q=cpu*2+k;
        state->lease=&typed_runtime.lease;state->token=&typed_runtime.tokens[q];
        state->target_hpa=domain_memory.backing.pages[14];state->root_index=q+2;
        state->environment_guard=typed_guard;state->guard_context=r;state->fatal=typed_fatal;state->fatal_context=r;
        r->sparse.typed_contexts[k]=state;
    }
    r->sparse.multiple=true;r->sparse.evict=false;
    r->sparse.typed_transactions=&typed_transactions;r->sparse.typed_transaction_context=r;
    r->sparse.contexts_ready=typed_contexts_ready;r->sparse.refill_begin=typed_refill_begin;r->sparse.refill_end=typed_refill_end;
    return true;
}
