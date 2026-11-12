#ifndef SYLE_CACHE_MAINTENANCE_H
#define SYLE_CACHE_MAINTENANCE_H
#include <stdbool.h>
#include "cache_page.h"
#include <stdint.h>
struct cache_cpu_snapshot {uint64_t cr0,cr3,cr4,pat,target;uint32_t apic;bool mtrr_wb;};
/* Caller guarantees this allocator-owned data page is not code, stack,
 * mailbox or paging structure; all guest entries are gated off. */
bool cache_maintenance_precheck(uint64_t target,struct cache_cpu_snapshot *);
bool cache_maintenance_same(const struct cache_cpu_snapshot *,const struct cache_cpu_snapshot *);
bool cache_maintenance_tlb(const struct cache_cpu_snapshot *);
bool cache_maintenance_cache(const struct cache_cpu_snapshot *);
bool cache_maintenance_lease(const struct cache_cpu_snapshot *,struct cache_page_lease *);
/* Reuses initial snapshot MTRR proof and rechecks CPU/PAT/CR3 stability plus
 * the complete current host mapping tree. Never dereferences target data. */
bool cache_maintenance_lease_current(const struct cache_cpu_snapshot *,unsigned current_type,
    struct cache_page_lease *);

/* Explicit WT preflight: initial WB host mapping plus local MTRR-WB proof,
 * full PAT validation and existing WT slot selection. Outputs are atomic on
 * success, unchanged on failure (caller can report BLOCKED). No PAT writes. */
bool cache_maintenance_precheck_type(uint64_t target,unsigned requested_type,
    struct cache_cpu_snapshot *,unsigned *slot,uint64_t *bits);
/* type=4 audits current WT, type=6 audits restored WB. Selection is bound to
 * the snapshot's full PAT; no WT-time MTRR reclassification of target. */
bool cache_maintenance_lease_current_type(const struct cache_cpu_snapshot *,unsigned type,
    unsigned slot,uint64_t bits,struct cache_page_lease *out);

#endif
