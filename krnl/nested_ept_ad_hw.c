#include "nested_ept_ad_hw.h"
#include "paging.h"
#include "../efi/src/exceptions.h"
struct nested_ad_record {
    struct nested_sparse_page self;
    struct nested_ad_record *next;
    uint64_t gpa;
    struct nested_ept_ad_trace trace;
};
_Static_assert(sizeof(struct nested_ad_record)<=4096,"AD trace record fits page");
static bool mutable(void *ctx,uint64_t gpa,uint8_t need,uint64_t *hpa,uint64_t **page,uint8_t *perm) {
    const uint64_t *readview;
    if(!nested_sparse_hw_resolve(ctx,gpa,need,hpa,&readview,perm)||!(*perm&2))return false;
    /* Explicit writable private-RAM mapping, never cast away const on a
     * guest-supplied pointer. The resolver already verified owned identity. */
    *page=paging_phys_to_virt(*hpa);
    return *page&&*page==readview;
}
void nested_ept_ad_cache_clear(struct nested_ept_ad_cache *c,struct nested_ept_sparse_hw *h) {
    struct nested_sparse_ops ops=nested_sparse_hw_ops();
    while(c->records){struct nested_ad_record *r=c->records;c->records=r->next;ops.free(h,r->self);}
}
bool nested_ept_ad_hw_init(struct nested_ept_ad_cache *c,struct nested_ept_sparse_hw *h,struct ept_fixture *f,unsigned bits) {
    *c=(struct nested_ept_ad_cache){.policy={bits,true,true}};
    *h=(struct nested_ept_sparse_hw){.fixture=f};
    struct nested_sparse_ops ops=nested_sparse_hw_ops();
    bool ok=nested_ept_sparse_init_mode(&h->tree,&ops,h,bits,true);
    h->eptp=h->tree.eptp;return ok;
}
enum nested_ad_action nested_ept_ad_tree_handle(struct nested_ept_ad_cache *c,struct nested_ept_sparse_hw *h,struct nested_ept_sparse *tree,uint64_t eptp12,uint64_t gpa,uint8_t need,struct nested_ept_result *out) {
    if(!out||!need||(need&~7u))return NESTED_AD_FATAL;
    uint64_t page=gpa&~UINT64_C(4095);
    struct nested_ept_result result;
    enum nested_sparse_lookup found=nested_ept_sparse_lookup(tree,gpa,need,&result);
    if(found==NESTED_SPARSE_HIT) {
        *out=result;
        struct nested_ad_record *saved=c->records;
        while(saved&&saved->gpa!=page)saved=saved->next;
        if(result.status==NESTED_EPT_L1_DENIED) {
            if(!saved||!nested_ept_ad_update_prefix(&saved->trace,mutable,h->fixture))return NESTED_AD_FATAL;
            return NESTED_AD_L1_DENIED;
        }
        bool tracked=false;
        if(result.status!=NESTED_EPT_OK||!(need&2)||
           !nested_ept_sparse_query_tracking(tree,gpa,&tracked)||!tracked)return NESTED_AD_FATAL;
        struct nested_ad_record *r=c->records;
        while(r&&r->gpa!=page)r=r->next;
        if(!r||!nested_ept_ad_update(&r->trace,true,mutable,h->fixture))return NESTED_AD_FATAL;
        return nested_ept_sparse_promote_write(tree,gpa)?NESTED_AD_RETRY:NESTED_AD_FATAL;
    }
    if(found!=NESTED_SPARSE_MISS)return NESTED_AD_FATAL;
    struct nested_ept_ad_trace trace;
    result=nested_ept_ad_walk_prefix(eptp12,page,1,nested_sparse_hw_resolve,h->fixture,&c->policy,&trace);
    result=nested_sparse_walk_access(result,need);
    *out=result;
    if(result.status==NESTED_EPT_L1_DENIED) {
        if(!nested_ept_ad_update_prefix(&trace,mutable,h->fixture))return NESTED_AD_FATAL;
        return NESTED_AD_L1_DENIED;
    }
    if(result.status!=NESTED_EPT_OK||!trace.count)return NESTED_AD_FATAL;
    struct nested_sparse_ops ops=nested_sparse_hw_ops();
    struct nested_sparse_page meta;
    if(!ops.alloc(h,&meta))return NESTED_AD_FATAL;
    struct nested_ad_record *record=meta.view;
    *record=(struct nested_ad_record){.self=meta,.next=c->records,.gpa=page,.trace=trace};
    c->records=record; /* retain ownership on any later failure */
    if(!nested_ept_ad_update(&trace,(need&2)!=0,mutable,h->fixture))return NESTED_AD_FATAL;
    bool leaf_dirty=(trace.entries[trace.count-1].raw&0x200)||(need&2);
    bool trap_write=(result.effective02&2)&&!leaf_dirty;
    return nested_ept_sparse_fill_tracked(tree,page,&result,trap_write)==NESTED_SPARSE_OK?NESTED_AD_RETRY:NESTED_AD_FATAL;
}
enum nested_ad_action nested_ept_ad_hw_handle(struct nested_ept_ad_cache *c,struct nested_ept_sparse_hw *h,uint64_t gpa,uint8_t need,struct nested_ept_result *out) {
    return nested_ept_ad_tree_handle(c,h,&h->tree,0x1005e,gpa,need,out);
}
bool nested_ept_ad_hw_fault(struct nested_ept_ad_cache *c,struct nested_ept_sparse_hw *h,uint64_t gpa,uint8_t need) {
    struct nested_ept_result ignored;
    return nested_ept_ad_hw_handle(c,h,gpa,need,&ignored)==NESTED_AD_RETRY;
}
bool nested_ept_ad_hw_reset(struct nested_ept_ad_cache *c,struct nested_ept_sparse_hw *h) {
    if(!nested_ept_sparse_reset(&h->tree))return false;
    nested_ept_ad_cache_clear(c,h);return true;
}
void nested_ept_ad_hw_destroy(struct nested_ept_ad_cache *c,struct nested_ept_sparse_hw *h) {
    if(!nested_ept_sparse_destroy(&h->tree))panic("AD INVALIDATION FAILED RETAIN");
    nested_ept_ad_cache_clear(c,h);
    if(h->allocated_pages)panic("AD ALLOCATION LEAK");
    h->eptp=0;
}
