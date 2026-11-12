/* L0 maintenance only. No shared backing is freed and no virtual INVEPT is
 * broadcast. Each caller arrives after verified physical L2 exit/leave. */
static struct {
    uint32_t stopped[2][2],command[2],acked[2],prepare,prepared,release[2];
    struct nested_domain_maintenance_ticket ticket[2];
    struct ad_entry_stamp next;
    uint64_t readonly_leaf;
} ad_maintenance;
static bool ad_wait(uint32_t *p){
    uint32_t lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    uint64_t start=((uint64_t)hi<<32)|lo;
    while(!__atomic_load_n(p,__ATOMIC_ACQUIRE)){
        if(__atomic_load_n(&domain_dispatch.fault_latch,__ATOMIC_ACQUIRE)||
           __atomic_load_n(&domain_ad_gate.fault,__ATOMIC_ACQUIRE))return false;
        __asm__ volatile("pause;rdtsc":"=a"(lo),"=d"(hi));
        if((((uint64_t)hi<<32)|lo)-start>UINT64_C(30000000000))return false;
    }
    return true;
}
static bool ad_checkpoint(struct nested_parallel_runtime *r,unsigned phase){
    if(phase>1)return false;
    unsigned cpu=r->cpu_index;
    __atomic_store_n(&ad_maintenance.stopped[phase][cpu],1,__ATOMIC_RELEASE);
    struct nested_domain_maintenance_ticket *ticket=&ad_maintenance.ticket[phase];
    if(cpu){
        if(!ad_wait(&ad_maintenance.command[phase]))goto fail;
        if(!phase&&!nested_owner_ad_local_reset(&r->ad,true))goto fail;
        if(!phase&&(!nested_parallel_trace_text(r,"NESTED.AD_RESET_LOCAL_INVEPT generation=")||
           !nested_parallel_trace_hex(r,r->sparse.tree.generation)))goto fail;
        if(!nested_domain_dispatch_maintenance_ack(&domain_dispatch,cpu,*ticket))goto fail;
        __atomic_store_n(&ad_maintenance.acked[phase],1,__ATOMIC_RELEASE);
        if(!phase){
            if(!ad_wait(&ad_maintenance.prepare)||
               !nested_owner_ad_advance_stamp(&r->ad,domain_ad_stamp,ad_maintenance.next))goto fail;
            __atomic_store_n(&ad_maintenance.prepared,1,__ATOMIC_RELEASE);
        }
        if(!ad_wait(&ad_maintenance.release[phase]))goto fail;
        return true;
    }
    if(!ad_wait(&ad_maintenance.stopped[phase][1])||
       !nested_domain_dispatch_maintenance_begin(&domain_dispatch,
           phase?NESTED_DOMAIN_AD_OBSERVE:NESTED_DOMAIN_AD_RESET,ticket)||
       domain_dispatch.domain.maintenance_targets!=3)goto fail;
    if(!phase){
        if(!ad_updater_gate_close(&domain_ad_gate,domain_ad_stamp)||
           !ad_updater_gate_drained(&domain_ad_gate,domain_ad_stamp))goto fail;
    }else if(!ad_updater_gate_idle(&domain_ad_gate,domain_ad_stamp))goto fail;
    __atomic_store_n(&ad_maintenance.command[phase],1,__ATOMIC_RELEASE);
    if((!phase&&!nested_owner_ad_local_reset(&r->ad,true))||
       !nested_domain_dispatch_maintenance_ack(&domain_dispatch,0,*ticket)||
       !ad_wait(&ad_maintenance.acked[phase]))goto fail;
    if(!phase){
        if(!nested_parallel_trace_text(r,"NESTED.AD_RESET_LOCAL_INVEPT generation=")||
           !nested_parallel_trace_hex(r,r->sparse.tree.generation))goto fail;
        /* All actual old 02 roots are empty and locally invalidated; old trace
         * records are gone. Neither hardware nor updater can rewrite bits. */
        uint64_t *oldpt=paging_phys_to_virt(domain_memory.backing.pages[18]);
        if((__atomic_load_n(&oldpt[15],__ATOMIC_ACQUIRE)&0x300)!=0x300)goto fail;
        if(!ad_updater_gate_drained(&domain_ad_gate,domain_ad_stamp)||
           domain_ad_stamp.epoch==UINT64_MAX)goto fail;
        for(unsigned l=0;l<4;l++){
            uint64_t *page=paging_phys_to_virt(domain_memory.backing.pages[15+l]);
            for(unsigned n=0;n<512;n++){
                uint64_t old=__atomic_load_n(&page[n],__ATOMIC_ACQUIRE);
                if(old&7)__atomic_fetch_and(&page[n],~UINT64_C(0x300),__ATOMIC_ACQ_REL);
            }
        }
        ad_maintenance.next=domain_ad_stamp;ad_maintenance.next.epoch++;
        __atomic_store_n(&ad_maintenance.prepare,1,__ATOMIC_RELEASE);
        if(!nested_owner_ad_advance_stamp(&r->ad,domain_ad_stamp,ad_maintenance.next)||
           !ad_wait(&ad_maintenance.prepared))goto fail;
        struct ad_entry_stamp next;
        if(!ad_updater_gate_reopen(&domain_ad_gate,domain_ad_stamp,true,&next)||
           next.epoch!=ad_maintenance.next.epoch)goto fail;
        domain_ad_stamp=next; /* AP passed prepare ACK and reads only after release. */
    }else{
        uint64_t *pt=paging_phys_to_virt(domain_memory.backing.pages[18]);
        ad_maintenance.readonly_leaf=__atomic_load_n(&pt[15],__ATOMIC_ACQUIRE);
        if((ad_maintenance.readonly_leaf&0x300)!=0x100||
           !ad_updater_gate_idle(&domain_ad_gate,domain_ad_stamp))goto fail;
    }
    /* Consumer admission opens LAST, after both owner stamps are ready. */
    if(!nested_domain_dispatch_maintenance_publish(&domain_dispatch,*ticket)||
       !nested_domain_dispatch_maintenance_complete(&domain_dispatch,*ticket))goto fail;
    __atomic_store_n(&ad_maintenance.release[phase],1,__ATOMIC_RELEASE);
    return true;
fail:
    ad_updater_gate_poison(&domain_ad_gate);
    nested_domain_dispatch_markfail(&domain_dispatch);
    return false;
}
