/* Two fixture invalidation barriers; no lock held while another owner runs. */
static struct {
    uint32_t stopped[2][2],release[2];
    uint64_t generation[2],fills[2],hpa[2];
    size_t live;
} owner_context_sync;
static bool context_wait(uint32_t *p){
    uint32_t a,d;__asm__ volatile("rdtsc":"=a"(a),"=d"(d));
    uint64_t start=((uint64_t)d<<32)|a;
    while(!__atomic_load_n(p,__ATOMIC_ACQUIRE)){
        if(__atomic_load_n(&domain_dispatch.fault_latch,__ATOMIC_ACQUIRE))return false;
        __asm__ volatile("pause;rdtsc":"=a"(a),"=d"(d));
        if((((uint64_t)d<<32)|a)-start>UINT64_C(30000000000))return false;
    }
    return true;
}
static bool context_remote_snapshot(bool verify){
    struct nested_owner_sparse *remote=&nested_runs[1].sparse;
    if(remote->contexts.count!=2||remote->contexts.poisoned)return false;
    unsigned seen=0;
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    const uint64_t second_key=0x1801e;
#else
    const uint64_t second_key=0x1401e;
#endif
    for(struct nested_ept_context_item *i=remote->contexts.items;i;i=i->next){
        unsigned index=i->key==0x1001e?0:i->key==second_key?1:2;
        if(index>1||(seen&(1u<<index)))return false;
        seen|=1u<<index;
        struct nested_ept_result r;
        if(nested_ept_sparse_lookup(&i->tree,0xf000,1,&r)!=NESTED_SPARSE_HIT||r.status!=NESTED_EPT_OK)return false;
        if(verify){
            if(owner_context_sync.generation[index]!=i->tree.generation||
               owner_context_sync.fills[index]!=i->tree.fill_count||
               owner_context_sync.hpa[index]!=r.hpa)return false;
        }else{
            owner_context_sync.generation[index]=i->tree.generation;
            owner_context_sync.fills[index]=i->tree.fill_count;
            owner_context_sync.hpa[index]=r.hpa;
        }
    }
    if(seen!=3)return false;
    if(verify)return owner_context_sync.live==remote->pool.live;
    owner_context_sync.live=remote->pool.live;return true;
}
static bool owner_context_checkpoint(struct nested_parallel_runtime *r){
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    const unsigned first=4;
#else
    const unsigned first=2;
#endif
    if(r->context_stage<first||r->context_stage>first+1)return false;
    unsigned phase=r->context_stage-first,cpu=r->cpu_index;
    __atomic_store_n(&owner_context_sync.stopped[phase][cpu],1,__ATOMIC_RELEASE);
    if(cpu)return context_wait(&owner_context_sync.release[phase]);
    return context_wait(&owner_context_sync.stopped[phase][1])&&context_remote_snapshot(false);
}
static bool owner_context_invalidated(struct nested_parallel_runtime *r){
    if(r->local_invalidations<1||r->local_invalidations>2)return false;
    unsigned phase=r->local_invalidations-1;
#ifdef VMX_NESTED_OWNER_EVICT_TEST
    if(r->context_stage!=phase+4)return false;
#else
    if(r->context_stage!=phase+2)return false;
#endif
    if(r->cpu_index)return __atomic_load_n(&owner_context_sync.release[phase],__ATOMIC_ACQUIRE)==1;
    if(!context_remote_snapshot(true))return false;
    __atomic_store_n(&owner_context_sync.release[phase],1,__ATOMIC_RELEASE);
    return true;
}
