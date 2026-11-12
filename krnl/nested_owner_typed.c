#include "nested_owner_typed.h"
#include "paging.h"
#include "vmx_memory.h"
static bool owner(struct nested_owner_typed *t) {
    if(!t||t->failed||!t->owner)return false;
    unsigned a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    struct nested_owner_sparse *s=t->owner;
    if(!s->pool.active||s->pool.fault||s->pool.owner!=(b>>24)||!s->initialized||
       s->destroyed||!t->tree||t->tree->hardware_ad)return false;
    if(!s->multiple)return t->tree==&s->tree&&t->eptp12==0x1001e&&s->selected12==0x1001e;
    if(s->evict||!s->contexts.initialized||s->contexts.poisoned||
       (t->eptp12!=0x1001e&&t->eptp12!=0x1401e))return false;
    for(struct nested_ept_context_item *i=s->contexts.items;i;i=i->next)
        if(&i->tree==t->tree)return i->key==t->eptp12;
    return false;
}
static bool live(struct nested_owner_typed *t) {
    return owner(t)&&t->lease&&t->token&&t->environment_guard&&
        t->environment_guard(t->guard_context)&&t->lease->phase==DTL_OPEN&&
        t->target_hpa==t->lease->inventory.host.target&&
        dtl_admit(t->lease,t->token,t->owner->pool.owner,t->root_index,t->nonce,
                  t->epoch,t->lease->type,true,false);
}
static bool fail(struct nested_owner_typed *t) {
    if(t){t->failed=true;if(t->fatal)t->fatal(t->fatal_context);}
    return false;
}
static bool resolve(void *context,uint64_t gpa,uint8_t need,uint64_t *hpa,
                    const uint64_t **view,uint8_t *permissions) {
    (void)need;
    struct nested_owner_typed *t=context;
    if(!live(t)||!gpa||(gpa&4095)||gpa>=0x40000)return false;
    struct ept_fixture *f=t->owner->fixture;
    if(!f)return false;
    for(unsigned l=0;l<3;l++){
        if(f->tables[l]==t->target_hpa)return false;
        const uint64_t *p=paging_phys_to_virt(f->tables[l]);
        if(!p||p[0]!=(f->tables[l+1]|7))return false;
    }
    if(f->tables[3]==t->target_hpa)return false;
    const uint64_t *pt=paging_phys_to_virt(f->tables[3]);
    unsigned n=gpa/4096;
    if(!pt)return false;
    *hpa=f->pages[n-1];*permissions=pt[n]&7;
    bool target=*hpa==t->target_hpa;
    unsigned type=target?t->lease->type:6;
    if((pt[n]&~UINT64_C(7))!=(*hpa|0x40|((uint64_t)type<<3))||
       !(*permissions&1)||(*permissions&~7))return false;
    if(target){
        /* A registered data page never supplies table bytes, even in WB phase. */
        if(gpa!=0xf000||(*permissions&4))return false;
        *view=0;return true;
    }
    if(!vmx_memory_region_wb(*hpa))return false;
    *view=paging_phys_to_virt(*hpa);
    return *view&&!((uintptr_t)*view&4095);
}
static bool admit(void *context,const struct nested_ept_result *r) {
    struct nested_owner_typed *t=context;
    if(!live(t)||!r||r->status!=NESTED_EPT_OK||!r->memory_explicit||
       !r->ignore_pat||r->walk_type!=6)return false;
    uint64_t hpa=r->hpa&~UINT64_C(4095);
    if(hpa==t->target_hpa)
        return (r->l1_gpa&~UINT64_C(4095))==0xf000&&
            r->leaf_type==t->lease->type&&!(r->effective02&4);
    return r->leaf_type==6&&vmx_memory_region_wb(hpa);
}
bool nested_owner_typed_bind(struct nested_owner_typed *t,struct nested_owner_sparse *s) {
    if(!s||!t||t->bound||t->failed||!s->fixture)return false;
    t->owner=s;t->tree=&s->tree;t->eptp12=0x1001e;
    if(!owner(t)||!t->environment_guard||!t->fatal||!(t->allow_wt?nested_ept_sparse_set_typed_admission_wt(&s->tree,admit,t):nested_ept_sparse_set_typed_admission(&s->tree,admit,t)))return fail(t);
    t->bound=true;return true;
}
bool nested_owner_typed_bind_context(struct nested_owner_typed *t,struct nested_owner_sparse *s,struct nested_ept_context_item *item) {
    if(!s||!t||!item||t->bound||t->failed||!s->fixture||!s->multiple||s->evict)return false;
    bool found=false;
    for(struct nested_ept_context_item *i=s->contexts.items;i;i=i->next)if(i==item)found=true;
    if(!found)return false;
    t->owner=s;t->tree=&item->tree;t->eptp12=item->key;
    if(!owner(t)||!t->environment_guard||!t->fatal||
       !(t->allow_wt?nested_ept_sparse_set_typed_admission_wt(t->tree,admit,t):
                       nested_ept_sparse_set_typed_admission(t->tree,admit,t)))return fail(t);
    t->bound=true;return true;
}
bool nested_owner_typed_miss(struct nested_owner_typed *t,uint64_t gpa,uint8_t access) {
    if(!t||!t->bound||!live(t)||t->owner->selected12!=t->eptp12||
       (t->owner->multiple&&(!t->owner->contexts.active||&t->owner->contexts.active->tree!=t->tree))||!access||(access&~7)||t->misses==UINT64_MAX)return fail(t);
    uint64_t page=gpa&~UINT64_C(4095);
    struct nested_ept_result r;
    if(nested_ept_sparse_lookup(t->tree,page,access,&r)!=NESTED_SPARSE_MISS)return fail(t);
    struct nested_ept_policy policy={.maxphys=t->owner->pool.maxphys};
    r=nested_ept_walk_memory(t->eptp12,page,1,resolve,t,&policy);
    r=nested_sparse_walk_access(r,access);
    if(r.status!=NESTED_EPT_OK||!admit(t,&r)||
       nested_ept_sparse_fill(t->tree,page,&r)!=NESTED_SPARSE_OK)return fail(t);
    t->misses++;return true;
}
bool nested_owner_typed_refresh(struct nested_owner_typed *t,uint64_t epoch,uint64_t nonce) {
    if(!t||!t->bound||!owner(t)||!t->lease||!t->token||!t->environment_guard||
       !t->environment_guard(t->guard_context)||(t->lease->phase!=DTL_STAMP&&t->lease->phase!=DTL_OPEN)||
       t->tree->leaves||t->tree->fill_count||t->tree->poisoned||
       t->epoch==UINT64_MAX||epoch!=t->epoch+1||nonce!=t->nonce||
       !t->token->live||!t->token->prepared||t->token->lease!=t->lease||
       t->token->owner!=t->owner->pool.owner||t->token->root_index!=t->root_index||
       t->token->nonce!=nonce||t->token->epoch!=epoch||
       t->root_index>=t->lease->inventory.roots||
       t->token->root_generation!=t->lease->inventory.root[t->root_index].generation||
       t->token->type!=t->lease->destination)return false;
    t->epoch=epoch;return true;
}
