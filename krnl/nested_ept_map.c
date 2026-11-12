#include "nested_ept_map.h"
#include <stddef.h>

bool nested_ept_map_build(struct nested_ept_map *out,uint64_t eptp12,
    nested_ept_resolve_fn resolve,void *context) {
    if(!out || !resolve) return false;
    struct nested_ept_map staged;
    for(unsigned i=0;i<NESTED_EPT_MAP_PAGES;i++) {
        struct nested_ept_result r=nested_ept_walk(eptp12,(uint64_t)i<<12,1,resolve,context);
        if(r.status==NESTED_EPT_OK && r.leaf_resolved) {
            /* The walker guarantees 4K/aligned/physical48 and readable legal
             * permissions. Preserve its complete EPT12 and EPT02 masks. */
            staged.leaves[i]=r.hpa|r.effective02|UINT64_C(0x30);
        } else if(r.status==NESTED_EPT_L1_DENIED &&
                  !r.leaf_resolved && r.effective12==0) {
            staged.leaves[i]=0;
        } else return false;
        staged.pages[i]=r;
    }
    /* No observable writes to out until every source page has validated.
     * This is transactional failure behavior, NOT concurrent atomic publish. */
    for(unsigned i=0;i<NESTED_EPT_MAP_PAGES;i++) {
        out->leaves[i]=staged.leaves[i];
        out->pages[i]=staged.pages[i];
    }
    return true;
}
struct nested_ept_result nested_ept_map_classify(const struct nested_ept_map *map,
    uint64_t l2_gpa,uint8_t access) {
    struct nested_ept_result bad={.status=NESTED_EPT_MALFORMED_UNSUPPORTED};
    if(!map || l2_gpa>=NESTED_EPT_MAP_LIMIT ||
       (access!=1 && access!=2 && access!=4)) return bad;
    struct nested_ept_result r=map->pages[l2_gpa>>12];
    if(!r.leaf_resolved) return r;
    uint64_t offset=l2_gpa&0xfff;
    r.l1_gpa+=offset;
    r.hpa+=offset;
    if(!(r.effective12&access)) r.status=NESTED_EPT_L1_DENIED;
    else if(!(r.effective02&access)) r.status=NESTED_EPT_L0_BACKING_DENIED;
    else r.status=NESTED_EPT_OK;
    return r;
}
