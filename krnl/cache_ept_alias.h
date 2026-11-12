#ifndef SYLE_CACHE_EPT_ALIAS_H
#define SYLE_CACHE_EPT_ALIAS_H
#include <stdint.h>
#include <stdbool.h>
struct cache_ept_alias_view {
    uint64_t *table[4]; /* borrowed writable CPU views: PML4, PDPT, PD, PT */
    uint64_t hpa[4],eptp;
};
enum cache_ept_alias_phase { CACHE_EPT_ALIAS_READY=1,CACHE_EPT_ALIAS_DETACHED,CACHE_EPT_ALIAS_PUBLISHED };
struct cache_ept_alias_plan {
    struct cache_ept_alias_view view;
    uint64_t original[4][512];
    uint64_t aliases[8],target;
    unsigned count;
    bool to_uc;
    enum cache_ept_alias_phase phase;
};
/* Narrow, complete four-table shape audit, not a general EPT walker.
 * EPTP WB/4-level/AD0; only upper index0 can be present, exactly owned next
 * table|7. Target aliases must be R-only WB/IPAT0 -> UC/IPAT1 or reverse.
 * Borrowed distinct table pages and target must be private/stable; caller
 * supplies their truthful HPA/view mapping. All HPAs are aligned <2^48.
 * Prepare failure leaves output unchanged. Plan requires about 16KiB caller-owned storage: use static/private
 * allocated storage, not a small kernel stack. Plan must not alias tables.
 * After PUBLISHED, prepare may be called again for the reverse transition.
 * Every call requires exclusive ownership: no CPU/hardware table mutations.
 * Detach/publish only edit entries: caller MUST separately coordinate vCPUs,
 * INVEPT, host aliases and cache-maintenance/lease ordering before reuse.
 * Successful detach pins the borrowed resources until caller completes the
 * transition. No allocation/free, privileged operation or rollback is done. */
bool cache_ept_alias_prepare(struct cache_ept_alias_plan *plan,
    const struct cache_ept_alias_view *view,uint64_t target,bool to_uc);
bool cache_ept_alias_detach(struct cache_ept_alias_plan *plan);
bool cache_ept_alias_publish(struct cache_ept_alias_plan *plan);
#endif
