#ifndef SYLE_NESTED_EPT_MAP_H
#define SYLE_NESTED_EPT_MAP_H
#include "nested_ept.h"
#define NESTED_EPT_MAP_PAGES 64u
#define NESTED_EPT_MAP_LIMIT UINT64_C(0x40000)
/* Complete bounded plan, not a live EPT or synchronization primitive.
 * No generation or implicit invalidation: the owner controls publication and
 * hardware INVEPT. Input tables/ownership map must remain stable during build. */
struct nested_ept_map {
    uint64_t leaves[NESTED_EPT_MAP_PAGES];
    struct nested_ept_result pages[NESTED_EPT_MAP_PAGES];
};
/* Read-walk all 64 pages. Any malformed/unsupported or L0-denied page rejects
 * the whole plan and preserves out. Nonpresent EPT12 pages become zero leaves.
 * Uses one bounded temporary plan (~2.5 KiB), no heap or recursive walk.
 * out must not overlap the callback's source tables or ownership metadata. */
bool nested_ept_map_build(struct nested_ept_map *out,uint64_t eptp12,
    nested_ept_resolve_fn resolve,void *context);
/* Classifies one exact address, not an arbitrary byte range. A caller checking
 * a cross-page operand must classify every touched page. High addresses are
 * rejected before indexing; no masking/aliasing into this bounded window. */
struct nested_ept_result nested_ept_map_classify(const struct nested_ept_map *map,
    uint64_t l2_gpa,uint8_t access);
#endif
