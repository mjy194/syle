#ifdef VMX_NESTED_DOMAIN_KICK_TEST
static bool icr(volatile uint32_t *,uint32_t,uint32_t);
#endif
/* N33C: L0 domain backing revoke, not virtual INVEPT broadcast.
 * Only BSP mutates source mappings; both CPUs are stopped in root. */
static struct {
    uint32_t warm[2],command,flushed,complete;
    struct nested_domain_reclaim_ticket ticket;
} domain_reclaim;
static bool domain_wait(uint32_t *word) {
    uint64_t start;
    uint32_t lo,hi;
    __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    start=((uint64_t)hi<<32)|lo;
    while(!__atomic_load_n(word,__ATOMIC_ACQUIRE)) {
        if(__atomic_load_n(&domain_dispatch.fault_latch,__ATOMIC_ACQUIRE))return false;
        __asm__ volatile("pause;rdtsc":"=a"(lo),"=d"(hi));
        if((((uint64_t)hi<<32)|lo)-start>UINT64_C(30000000000))return false;
    }
    return true;
}
static bool domain_local_flush(struct nested_parallel_runtime *r) {
    uint64_t roots[2]={r->vcpu.fixture.eptp,r->composed_eptp};
    for(unsigned n=0;n<2;n++) {
        struct {uint64_t eptp,reserved;} descriptor={roots[n],0};
        unsigned char failed;
        __asm__ volatile("invept %1,%2;setna %0":"=qm"(failed):
                         "m"(descriptor),"r"(UINT64_C(1)):"cc","memory");
        if(failed)return false;
    }
    return true;
}
static bool domain_reclaim_checkpoint(struct nested_parallel_runtime *r) {
    unsigned cpu=r->cpu_index;
    __atomic_store_n(&domain_reclaim.warm[cpu],1,__ATOMIC_RELEASE);
    if(cpu) {
        if(!domain_wait(&domain_reclaim.command)||!domain_local_flush(r)||
           !nested_domain_dispatch_reclaim_ack(&domain_dispatch,cpu,domain_reclaim.ticket))
            goto fail;
        __atomic_store_n(&domain_reclaim.flushed,1,__ATOMIC_RELEASE);
        if(!domain_wait(&domain_reclaim.complete))goto fail;
        return true;
    }
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    uint32_t *heartbeat=(uint32_t *)((unsigned char *)paging_phys_to_virt(nested_shared)+0x108);
    if(!domain_wait(heartbeat)||
       !icr((volatile uint32_t *)(uintptr_t)box.lapic,box.expected_id,0x41))goto fail;
#endif
    if(!domain_wait(&domain_reclaim.warm[1])||
       !nested_domain_dispatch_reclaim_begin(&domain_dispatch,&domain_reclaim.ticket)||
       domain_dispatch.domain.targets!=3 ||
       !nested_domain_memory_remap_data(&domain_memory,0xf000,true))goto fail;
    /* Borrowed fixture snapshots are also potential refill sources. Refresh
     * both while no owner can enter, before attesting publish complete. */
    for(unsigned i=0;i<2;i++)
        nested_runs[i].vcpu.fixture.pages[14]=domain_memory.backing.pages[14];
    if(!nested_domain_dispatch_reclaim_publish(&domain_dispatch,domain_reclaim.ticket))goto fail;
    __atomic_store_n(&domain_reclaim.command,1,__ATOMIC_RELEASE);
    if(!domain_local_flush(r)||
       !nested_domain_dispatch_reclaim_ack(&domain_dispatch,0,domain_reclaim.ticket)||
       !domain_wait(&domain_reclaim.flushed)||
       !nested_domain_dispatch_reclaim_complete(&domain_dispatch,domain_reclaim.ticket))goto fail;
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    __atomic_store_n((uint64_t *)((unsigned char *)paging_phys_to_virt(nested_shared)+0x100),1,__ATOMIC_RELEASE);
#endif
    __atomic_store_n(&domain_reclaim.complete,1,__ATOMIC_RELEASE);
    return true;
fail:
    nested_domain_dispatch_markfail(&domain_dispatch);
    return false;
}
