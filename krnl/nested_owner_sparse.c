#include "nested_owner_sparse.h"
#include "paging.h"
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
#include "nested_owner_atomic.h"
#endif
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
#include "nested_owner_dynamic_typed.h"
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
#include "nested_owner_typed.h"
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
#include "nested_owner_ad.h"
#endif
static unsigned cpu(void) {
    unsigned a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    return b>>24;
}
static bool owner(struct nested_owner_sparse *s){return s&&s->pool.active&&!s->pool.fault&&s->pool.owner==cpu();}
static bool alloc(void *ctx,struct nested_sparse_page *p){
    struct nested_owner_sparse *s=ctx;
    if(!owner(s)||!nested_owner_pool_alloc(&s->pool,p))return false;
    if(s->pool.live>s->peak_live)s->peak_live=s->pool.live;
    return true;
}
static void release(void *ctx,struct nested_sparse_page p){
    struct nested_owner_sparse *s=ctx;
    if(!owner(s)){s->pool.fault=true;return;}
    nested_owner_pool_free(&s->pool,p);
}
static bool flush(void *ctx,uint64_t eptp){
    if(!owner(ctx))return false;
    struct {uint64_t eptp,reserved;} d={eptp,0};unsigned char bad;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(bad):"m"(d),"r"(UINT64_C(1)):"cc","memory");
    return !bad;
}
static bool resolve(void *ctx,uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *perm){
    (void)need;struct nested_owner_sparse *s=ctx;
    if(!owner(s)||!gpa||(gpa&4095)||gpa>=UINT64_C(64)*4096)return false;
    struct ept_fixture *f=s->fixture;
    for(unsigned l=0;l<3;l++){
        uint64_t *t=paging_phys_to_virt(f->tables[l]);
        if(!t||t[0]!=(f->tables[l+1]|7))return false;
    }
    unsigned n=gpa>>12;
    uint64_t *pt=paging_phys_to_virt(f->tables[3]);
    if(!pt||(pt[n]&~UINT64_C(7))!=(f->pages[n-1]|0x30))return false;
    *hpa=f->pages[n-1];*view=paging_phys_to_virt(*hpa);*perm=pt[n]&7;
    return *hpa&&*view&&*perm;
}
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
struct nested_owner_typed *nested_owner_sparse_typed_context(struct nested_owner_sparse *s,uint64_t key){
    if(!s||!s->multiple)return NULL;
    if(key==0x1001e)return s->typed_contexts[0];
    if(key==0x1401e)return s->typed_contexts[1];
    return NULL;
}
bool nested_owner_sparse_bootstrap_contexts(struct nested_owner_sparse *s,const struct nested_sparse_ops *ops,void *allocator_context){
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
    if(!owner(s)||!s->initialized||s->destroyed||!s->fixture||
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
       !s->dynamic_typed||s->selected12!=s->dynamic_typed->initial_key||
       !nested_owner_catalog_key(s->dynamic_typed,s->selected12)||
#else
       s->selected12!=0x1001e||
#endif
       !s->multiple||!s->evict||s->contexts.initialized||s->creations||
       !s->contexts_ready||!s->typed_transactions||!s->dynamic_typed||
       !s->dynamic_typed->bridge||s->dynamic_typed->bound||s->dynamic_typed->failed||
       !s->dynamic_typed->environment_guard||!s->dynamic_typed->fatal||
       !s->dynamic_typed->bridge->metadata_lock||!s->dynamic_typed->bridge->metadata_unlock||
       s->typed_transactions!=typed_root_registry_transactions()||
       s->typed_transaction_context!=s->dynamic_typed->bridge)return false;
    if(!nested_ept_contexts_init_budget(&s->contexts,ops,allocator_context,s->pool.maxphys,2,true)||
       !nested_ept_contexts_set_transactions(&s->contexts,s->typed_transactions,s->typed_transaction_context)||
       nested_ept_contexts_select(&s->contexts,s->selected12)!=NESTED_CONTEXT_OK)return false;
    s->creations=1;
    return nested_owner_dynamic_typed_bind(s->dynamic_typed,s)&&s->contexts_ready(s);
#else
    /* typed_configure has already supplied all state. No separate preparation
     * callback: accepting an unconfigured bootstrap must never require a
     * dummy callback just to reach the real transaction installation. */
    if(!owner(s)||!s->initialized||s->destroyed||!s->fixture||s->selected12!=0x1001e||
       !s->multiple||s->evict||s->contexts.initialized||s->creations||
       !s->contexts_ready||!s->refill_begin||!s->refill_end||!s->typed_transactions||
       !s->typed_contexts[0]||!s->typed_contexts[1]||s->typed_contexts[0]==s->typed_contexts[1]||
       s->typed_contexts[0]->bound||s->typed_contexts[1]->bound||
       !s->typed_contexts[0]->environment_guard||!s->typed_contexts[1]->environment_guard||
       !s->typed_contexts[0]->fatal||!s->typed_contexts[1]->fatal)return false;
    if(!nested_ept_contexts_init_budget(&s->contexts,ops,allocator_context,s->pool.maxphys,2,false)||
       !nested_ept_contexts_set_transactions(&s->contexts,s->typed_transactions,s->typed_transaction_context))return false;
    for(unsigned i=0;i<2;i++){
        uint64_t key=i?0x1401e:0x1001e;
        if(nested_ept_contexts_select(&s->contexts,key)!=NESTED_CONTEXT_OK||
           !nested_owner_typed_bind_context(s->typed_contexts[i],s,s->contexts.active))return false;
        s->creations++;
    }
    if(nested_ept_contexts_select(&s->contexts,0x1001e)!=NESTED_CONTEXT_OK)return false;
    return s->contexts_ready(s);
#endif
}
#endif
bool nested_owner_sparse_start(struct nested_owner_sparse *s,struct ept_fixture *f){
    if(!owner(s)||!f||s->initialized||s->destroyed)return false;
    s->fixture=f;
    struct nested_sparse_ops ops={alloc,release,flush};
    /* Even failed init may own root pages. Cleanup must see initialized. */
    s->initialized=true;
    s->selected12=0x1001e;
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    s->selected12=0x1005e;
    return nested_owner_atomic_bootstrap(s,&ops,s);
#endif
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    if(!s->dynamic_typed)return false;
    s->selected12=s->dynamic_typed->initial_key;
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
    if(s->multiple||!s->atomic_ad)return false;
    s->selected12=0x1005e;
    return nested_ept_sparse_init_mode(&s->tree,&ops,s,s->pool.maxphys,true)&&
        nested_owner_ad_bind(s->atomic_ad,s);
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
    return nested_owner_sparse_bootstrap_contexts(s,&ops,s);
#else
    if(s->multiple||!s->typed)return false;
    return nested_ept_sparse_init(&s->tree,&ops,s,s->pool.maxphys)&&nested_owner_typed_bind(s->typed,s);
#endif
#endif
    if(s->multiple){
        if(!nested_ept_contexts_init_budget(&s->contexts,&ops,s,s->pool.maxphys,2,s->evict)||
           nested_ept_contexts_select(&s->contexts,s->selected12)!=NESTED_CONTEXT_OK)return false;
        s->creations=1;return true;
    }
    return nested_ept_sparse_init(&s->tree,&ops,s,s->pool.maxphys);
}
bool nested_owner_sparse_miss(struct nested_owner_sparse *s,uint64_t gpa,uint8_t access){
    if(!owner(s)||!s->initialized||s->destroyed||!access||(access&~7)||s->misses==UINT64_MAX)return false;
#ifdef VMX_NESTED_OWNER_AD_TEST
    if(!s->atomic_ad||!nested_owner_ad_fault(s->atomic_ad,gpa,access))return false;
    s->misses++;return true;
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
    if(!s->dynamic_typed||!nested_owner_dynamic_typed_miss(s->dynamic_typed,gpa,access))return false;
#else
    struct nested_owner_typed *typed=nested_owner_sparse_typed_context(s,s->selected12);
    if(!typed||!s->refill_begin||!s->refill_end||!s->refill_begin(s))return false;
    bool filled=nested_owner_typed_miss(typed,gpa,access);
    bool ended=s->refill_end(s); /* Always discharge the whole-call permit. */
    if(!filled||!ended)return false;
#endif
#else
    if(!s->typed||!nested_owner_typed_miss(s->typed,gpa,access))return false;
#endif
    s->misses++;return true;
#endif
    struct nested_ept_sparse *tree=s->multiple?nested_ept_contexts_active(&s->contexts):&s->tree;
    if(!tree)return false;
    struct nested_ept_result r;
    if(nested_ept_sparse_lookup(tree,gpa,access,&r)!=NESTED_SPARSE_MISS)return false;
    r=nested_ept_walk(s->selected12,gpa&~UINT64_C(4095),1,resolve,s);
    r=nested_sparse_walk_access(r,access);
    /* A only supplies successful lazy fills. L1 faults are NOT swallowed or
     * mislabeled: unsupported/denied/backing-failure stops this profile. */
    if(r.status!=NESTED_EPT_OK||nested_ept_sparse_fill(tree,gpa&~UINT64_C(4095),&r)!=NESTED_SPARSE_OK)return false;
    s->misses++;return true;
}
bool nested_owner_sparse_select(struct nested_owner_sparse *s,uint64_t key){
    if(!owner(s)||!s->multiple||!s->initialized||s->destroyed)return false;
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    if(!nested_owner_atomic_stopped(s->atomic_catalog)||!nested_owner_atomic_allowed(s->atomic_catalog,key))return false;
#endif
    bool existed=false;
    for(struct nested_ept_context_item *i=s->contexts.items;i;i=i->next)
        if(i->key==key)existed=true;
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
    #ifdef VMX_NESTED_ROOT_CATALOG_TEST
    if(!nested_owner_catalog_key(s->dynamic_typed,key))return false;
#else
    if(key!=0x1001e&&key!=0x1401e&&key!=0x1801e)return false;
#endif
#else
    if(!nested_owner_sparse_typed_context(s,key)||!existed)return false;
#endif
#endif
    bool evicting=!existed&&s->contexts.count==s->contexts.max_resident;
    if((!existed&&s->creations==UINT64_MAX)||(evicting&&s->evictions==UINT64_MAX))return false;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    s->selecting12=key;
#endif
    if(nested_ept_contexts_select(&s->contexts,key)!=NESTED_CONTEXT_OK)return false;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    s->selecting12=0;
#endif
    if(!existed)s->creations++;
    if(evicting)s->evictions++;
    s->selected12=key;
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    if(!nested_owner_catalog_selected(s->dynamic_typed,key))return false;
#endif
    return true;
}
bool nested_owner_sparse_invalidate(struct nested_owner_sparse *s,uint64_t type,uint64_t key){
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    (void)s;(void)type;(void)key;return false; /* No partial/global A/D reset authority. */
#endif
    return owner(s)&&s->multiple&&s->initialized&&!s->destroyed&&
        nested_ept_contexts_invalidate(&s->contexts,type,key)==NESTED_CONTEXT_OK;
}
bool nested_owner_sparse_reset(struct nested_owner_sparse *s){
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    (void)s;return false;
#endif
    if(!owner(s)||!s->initialized||s->destroyed)return false;
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
    if(!s->multiple)return false;
    return nested_ept_contexts_invalidate(&s->contexts,2,0)==NESTED_CONTEXT_OK;
#else
    return nested_ept_sparse_reset(&s->tree);
#endif
}
bool nested_owner_sparse_stop(struct nested_owner_sparse *s){
    if(!owner(s)||!s->initialized||s->destroyed)return false;
    bool ok;
    if(s->multiple){
        /* Caller has actually VMCLEARed every VMCS02 reference. */
        nested_ept_contexts_release_all_after_clear(&s->contexts);
        s->hardware_item=0;
        ok=nested_ept_contexts_destroy(&s->contexts);
    }else ok=nested_ept_sparse_destroy(&s->tree);
#ifdef VMX_NESTED_OWNER_AD_TEST
    if(!ok||!nested_owner_ad_release_records(s->atomic_ad,true))return false;
#endif
    if(!ok||s->pool.fault||s->pool.live)return false;
    s->destroyed=true;return true;
}
