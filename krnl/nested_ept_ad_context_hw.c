#include "nested_ept_ad_context_hw.h"
_Static_assert(sizeof(struct nested_ept_ad_cache)<=sizeof(((struct nested_ept_context_item *)0)->attachment),"AD sidecar fits opaque context storage");
/* Byte copies avoid treating a declared opaque array as an aliased struct. */
static struct nested_ept_ad_cache cache_load(const struct nested_ept_context_item *item) {
    struct nested_ept_ad_cache c;
    unsigned char *d=(void *)&c;const unsigned char *s=(const void *)&item->attachment;
    for(unsigned i=0;i<sizeof c;i++)d[i]=s[i];
    return c;
}
static void cache_store(struct nested_ept_context_item *item,const struct nested_ept_ad_cache *c) {
    unsigned char *d=(void *)&item->attachment;const unsigned char *s=(const void *)c;
    for(unsigned i=0;i<sizeof *c;i++)d[i]=s[i];
}
void nested_ad_context_lifecycle(void *ctx,struct nested_ept_context_item *item,enum nested_context_event event) {
    struct nested_ept_sparse_hw *base=ctx;
    struct nested_ept_ad_cache c;
    if(event==NESTED_CONTEXT_INIT)c=(struct nested_ept_ad_cache){.policy={item->tree.maxphys,true,true}};
    else {c=cache_load(item);nested_ept_ad_cache_clear(&c,base);}
    cache_store(item,&c);
}
enum nested_ad_action nested_ad_context_handle(struct nested_ept_sparse_hw *base,struct nested_ept_context_item *item,uint64_t gpa,uint8_t need,struct nested_ept_result *out) {
    if(!item||!item->attachment_initialized)return NESTED_AD_FATAL;
    if(!item->tree.hardware_ad) {
        struct nested_ept_result r;
        enum nested_sparse_lookup found=nested_ept_sparse_lookup(&item->tree,gpa,need,&r);
        if(found!=NESTED_SPARSE_MISS)return NESTED_AD_FATAL;
        const struct nested_ept_policy policy={item->tree.maxphys,true,true};
        r=nested_ept_walk_ad0_ignored(item->key,gpa&~UINT64_C(4095),1,
            nested_sparse_hw_resolve,base->fixture,&policy);
        r=nested_sparse_walk_access(r,need);*out=r;
        if(r.status!=NESTED_EPT_OK)return NESTED_AD_FATAL;
        return nested_ept_sparse_fill(&item->tree,gpa&~UINT64_C(4095),&r)==NESTED_SPARSE_OK?
            NESTED_AD_RETRY:NESTED_AD_FATAL;
    }
    struct nested_ept_ad_cache c=cache_load(item);
    enum nested_ad_action action=nested_ept_ad_tree_handle(&c,base,&item->tree,item->key,gpa,need,out);
    /* Even failure can retain a newly owned trace page. Publish its ownership
     * before the caller stops and invokes cleanup. */
    cache_store(item,&c);return action;
}
