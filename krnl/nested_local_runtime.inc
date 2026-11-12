/* Exactly one native L1 writer; AP is stopped after verified physical_leave.
 * No shared backing retirement is authorized by this local INVEPT sequence. */
static struct {
    uint32_t stopped[2],release_ap;
    uint64_t ap_generation,ap_fills,ap_hpa;
    size_t ap_live;
} local_invalidation;
static bool local_wait(uint32_t *p) {
    uint32_t a,d;
    __asm__ volatile("rdtsc":"=a"(a),"=d"(d));
    uint64_t start=((uint64_t)d<<32)|a;
    while(!__atomic_load_n(p,__ATOMIC_ACQUIRE)){
        if(__atomic_load_n(&domain_dispatch.fault_latch,__ATOMIC_ACQUIRE))return false;
        __asm__ volatile("pause;rdtsc":"=a"(a),"=d"(d));
        if((((uint64_t)d<<32)|a)-start>UINT64_C(30000000000))return false;
    }
    return true;
}
static bool domain_local_checkpoint(struct nested_parallel_runtime *r) {
    unsigned cpu=r->cpu_index;
    __atomic_store_n(&local_invalidation.stopped[cpu],1,__ATOMIC_RELEASE);
    if(cpu)return local_wait(&local_invalidation.release_ap);
    if(!local_wait(&local_invalidation.stopped[1]))return false;
    struct nested_owner_sparse *remote=&nested_runs[1].sparse;
    struct nested_ept_result result;
    if(nested_ept_sparse_lookup(&remote->tree,0xf000,1,&result)!=NESTED_SPARSE_HIT||
       result.status!=NESTED_EPT_OK)return false;
    local_invalidation.ap_generation=remote->tree.generation;
    local_invalidation.ap_fills=remote->tree.fill_count;
    local_invalidation.ap_live=remote->pool.live;
    local_invalidation.ap_hpa=result.hpa;
    return true;
}
static bool domain_local_invalidated(struct nested_parallel_runtime *r) {
    if(r->local_invalidations!=1)return false;
    if(r->cpu_index)return __atomic_load_n(&local_invalidation.release_ap,__ATOMIC_ACQUIRE)==1;
    struct nested_owner_sparse *remote=&nested_runs[1].sparse;
    struct nested_ept_result result;
    if(nested_ept_sparse_lookup(&remote->tree,0xf000,1,&result)!=NESTED_SPARSE_HIT||
       result.status!=NESTED_EPT_OK||result.hpa!=local_invalidation.ap_hpa||
       remote->tree.generation!=local_invalidation.ap_generation||
       remote->tree.fill_count!=local_invalidation.ap_fills||
       remote->pool.live!=local_invalidation.ap_live)return false;
    __atomic_store_n(&local_invalidation.release_ap,1,__ATOMIC_RELEASE);
    return true;
}
