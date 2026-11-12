#ifndef SYLE_NESTED_EPT_AD_H
#define SYLE_NESTED_EPT_AD_H
#include "nested_ept.h"
struct nested_ept_ad_entry {
    uint64_t entry_gpa,raw,table_hpa;
    bool leaf;
};
struct nested_ept_ad_trace {
    unsigned count,maxphys,leaf_level;
    struct nested_ept_ad_entry entries[4];
};
/* Only WB/four-level/EPTP.AD=1 (low bits 0x5e). Reuses the policy walker,
 * including optional 2M/1G leaves. Success publishes trace; failure clears it.
 * No writeback during walk. L0-owned map and EPT12 tables must stay stable. */
struct nested_ept_result nested_ept_ad_walk(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy,struct nested_ept_ad_trace *trace);
/* Explicit mutable owned-page resolver, not a const cast. Returns an entire
 * writable 4K RAM page with trusted host view and EPT01 permissions. */
typedef bool (*nested_ept_mutable_resolve_fn)(void *context,uint64_t page_gpa,
    uint8_t need,uint64_t *page_hpa,uint64_t **whole_page,uint8_t *perm01);
/* Validate ALL entries/pointers/HPA/permissions and non-A/D identities before
 * writing anything. Require R+W EPT01 for every table page. Dirty requires W
 * permission throughout EPT12. Then set A on all levels and D on the leaf
 * only when dirty; preserve A/D already observed in the original trace.
 * Callers serialize against guest table mutation. No rollback/concurrency
 * protocol: after prevalidation writes cannot fail for trusted owned RAM.
 * A false return performs no writes. This does not emulate failed-walk A
 * side effects, execute-only mappings, or hardware EPT02 A/D management. */
bool nested_ept_ad_update(const struct nested_ept_ad_trace *trace,bool dirty,
    nested_ept_mutable_resolve_fn resolve,void *context);
/* Deterministic failed-walk A policy: OK gives full trace; L1_DENIED gives
 * validated present entries preceding a nonpresent terminal (count 0..3,
 * leaf_level=0), or a full legal permission-denied path. The nonpresent entry
 * is excluded. MALFORMED/L0 failure still clears trace and is not reflected.
 * This is a chosen permitted subset, not exact microarchitectural A timing. */
struct nested_ept_result nested_ept_ad_walk_prefix(uint64_t eptp12,uint64_t l2_gpa,
    uint8_t access,nested_ept_resolve_fn resolve,void *context,
    const struct nested_ept_policy *policy,struct nested_ept_ad_trace *trace);
/* Strictly OR A on all validated present-prefix entries; never newly set D.
 * Current D is preserved (not resurrected from old trace if cleared).
 * Empty valid prefix succeeds without resolving/writing. Same serialization
 * and full prevalidation/no partial writes contract as the full updater. */
bool nested_ept_ad_update_prefix(const struct nested_ept_ad_trace *trace,
    nested_ept_mutable_resolve_fn resolve,void *context);
#endif
