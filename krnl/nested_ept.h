#ifndef SYLE_NESTED_EPT_H
#define SYLE_NESTED_EPT_H
#include <stdbool.h>
#include <stdint.h>

/* Pure four-level/WB EPT12 walker and EPT02 composition planner.
 * Never casts an L1 GPA to a host pointer and never installs an EPT entry.
 * These legacy/policy APIs exclude A/D (explicit opt-in is in nested_ept_ad.h).
 * No execute-only, #VE, MBEC or physical addresses >= 2^48.
 * The legacy wrapper remains strict 4K-only; explicit policy enables PS leaves. */
enum nested_ept_status {
    NESTED_EPT_OK,
    NESTED_EPT_L1_DENIED,
    NESTED_EPT_L0_BACKING_DENIED,
    NESTED_EPT_MALFORMED_UNSUPPORTED,
    /* Returned only by the explicit classified API. */
    NESTED_EPT_MISCONFIG,
    NESTED_EPT_UNSUPPORTED
};
struct nested_ept_result {
    enum nested_ept_status status;
    uint64_t l1_gpa, hpa;
    uint8_t effective12, effective02;
    bool leaf_resolved;
    bool memory_explicit,ignore_pat;
    uint8_t leaf_type,walk_type;
};
/* Resolve one whole guest-physical page only through an L0-owned-RAM map.
 * page_gpa and returned page_hpa are page bases. A true result supplies
 * permissions even if need is not allowed; the walker enforces permissions.
 * false means no usable L0 backing. For table reads the returned pointer
 * must refer to an entire, readable, 4096-aligned owned RAM page. Leaf
 * pointers may be NULL; the walker neither dereferences nor returns them.
 * The caller must keep table contents and the ownership map stable for a walk.
 * need is R=1 for a table and the requested access (1/2/4) for leaf backing. */
typedef bool (*nested_ept_resolve_fn)(void *context, uint64_t page_gpa,
    uint8_t need, uint64_t *page_hpa, const uint64_t **page_pointer,
    uint8_t *perm01);
struct nested_ept_policy {
    unsigned maxphys; /* 12..48; bounds guest/table/backing physical addresses */
    bool allow_2m,allow_1g;
};
/* Large EPT12 leaves only select an L1 4K backing page. Every requested
 * address resolves that page through L0; no contiguous EPT02 huge mapping or
 * permission guarantee is inferred for the rest of the large-page range. */
struct nested_ept_result nested_ept_walk_policy(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy);
/* Explicit architectural AD0 inherited-bit mode: EPTP must have AD=0
 * (low 0x1e). Entry bits 8/9 are ignored at every level, including nonleaf D.
 * No writes or A/D trace are produced. Other legality/policy checks remain.
 * Strict old walk/policy APIs intentionally keep rejecting those bits. */
struct nested_ept_result nested_ept_walk_ad0_ignored(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy);
/* Explicit memory-type parsing, AD0 inherited A/D bits ignored. Legal
 * UC/WB EPTP and leaf UC/WC/WT/WP/WB are decoded, not hardware-admitted.
 * The caller must apply an independent physical-memory/cache policy. */
struct nested_ept_result nested_ept_walk_memory(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy);
struct nested_ept_result nested_ept_walk(uint64_t eptp12, uint64_t l2_gpa,
    uint8_t access, nested_ept_resolve_fn resolve, void *context);
/* Explicit conservative architectural fault classification, WB/AD0 subset.
 * MISCONFIG only for verified structural architectural violations. Legal
 * but unimplemented features, unknown fields and invalid API/EPTP inputs
 * return UNSUPPORTED; the caller must not reflect those as reason 49.
 * Policy flags express implementation support, NOT advertised virtual CPU
 * capabilities. In particular execute-only/disabled large pages are not
 * declared architectural misconfigurations by this API.
 * AD0 A/D bits and PT bit7 are ignored. Old APIs remain strict/unchanged. */
struct nested_ept_result nested_ept_walk_classified(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy);
/* Bounded three-repair diagnostic sequencing, separate from architectural
 * INVEPT validity. Shared sparse tests have a different checkpoint sequence. */
static inline bool nested_misconfig_repair_pending(unsigned invalidations,unsigned exits) {
    return invalidations<3 && exits==invalidations+1;
}
/* Restricted virtual single-context INVEPT descriptor classification. */
enum nested_invept_status { NESTED_INVEPT_VALID, NESTED_INVEPT_INVALID, NESTED_INVEPT_UNSUPPORTED };
static inline enum nested_invept_status nested_invept_check(uint64_t type,uint64_t eptp,uint64_t reserved) {
    if(type!=1||reserved||(eptp&4095)!=0x1e||(eptp>>48)) return NESTED_INVEPT_INVALID;
    return eptp==0x1001e?NESTED_INVEPT_VALID:NESTED_INVEPT_UNSUPPORTED;
}

#endif
