#ifndef SYLE_NESTED_EPT_ATOMIC_H
#define SYLE_NESTED_EPT_ATOMIC_H
#include "nested_ept.h"
#include "ad_entry_merge.h"
struct nested_ept_atomic_ops {
    bool (*guard)(void *,struct ad_entry_stamp);
    /* Authorizes whole writable, aligned, pinned WB host RAM independently
     * of the guest's read-only EPT-table aliases. Stable for the entire call.
     * Caller proves ownership, WB physical mapping, whole-call pin and that ALL
     * guest aliases forbid structural writes. This callback is authority supplied
     * by the monitor, not a physical audit performed by this module. */
    bool (*table)(void *,uint64_t gpa,uint64_t *hpa,uint64_t **page);
    bool (*leaf)(void *,uint64_t gpa,uint64_t *hpa,uint8_t *permissions);
    /* Both NULL selects real acquire-load/acq_rel-CAS. Inject both or neither.
     * Injected operations must be linearizable; CAS false has made no write. */
    uint64_t (*load)(void *,uint64_t *);
    bool (*cas)(void *,uint64_t *,uint64_t expected,uint64_t desired);
};
struct nested_ept_atomic_entry {
    uint64_t *pointer;
    uint64_t entry_gpa,table_hpa,raw;
    bool leaf;
    unsigned depth,leaf_shift; /* exact 0=PML4..3=PT; 0=table,12=4K,21=2M,30=1G */
};
struct nested_ept_atomic_trace {
    struct ad_entry_stamp stamp;
    unsigned count;
    struct nested_ept_atomic_entry entry[4];
};
enum nested_ept_atomic_status {
    NEA_DONE,NEA_INVALID,NEA_UNSUPPORTED,NEA_STALE,NEA_REWALK,NEA_RETRY,NEA_BACKING
};
struct nested_ept_atomic_result {
    enum nested_ept_atomic_status status;
    struct nested_ept_result translation;
    bool changed; /* This call changed entries; failure may have partial effects. */
};
/* Restricted WB, four-level, 4K, AD1. Caller ACQUIRES updater lease and pins all
 * tables before this call and RELEASES afterwards; this function never does so.
 * guard validates that live lease's full stamp, not merely gate openness.
 * Structural entries immutable while pinned; only atomic A/D writers allowed.
 * Nonpresent prefix and legal permission denial set A only; successful write
 * additionally sets leaf D. No old trace D is ever replayed. Unsupported large
 * leaves / malformed format stop fail-closed, never synthesize EPT misconfig.
 * Failure can follow partial CAS; no rollback. Caller must stop/retry according
 * to ownership protocol, never publish a successful translation on failure. */
struct nested_ept_atomic_result nested_ept_atomic_walk(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned maxphys,
    const struct nested_ept_atomic_ops *,void *,struct ad_entry_stamp,
    struct nested_ept_atomic_trace *);
/* Separate N41A opt-in: requires WB/IPAT1 at every present leaf, returns
 * explicit WB walk/leaf types. Same AD1-only ownership/lease contract as above.
 * Legacy API remains IPAT0-only with its original non-explicit result. */
struct nested_ept_atomic_result nested_ept_atomic_walk_wb_ipat(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned maxphys,
    const struct nested_ept_atomic_ops *,void *,struct ad_entry_stamp,
    struct nested_ept_atomic_trace *);
/* Explicit N44 opt-in, same immutable-source lease contract. Allows 2 MiB
 * PDE leaves, still returns one separately authorized 4 KiB backing page.
 * No contiguous/whole-large-page backing guarantee and no 1 GiB support.
 * Selecting this entry point is software capability policy, not evidence of
 * physical large-page support; composed EPT02 leaves remain 4 KiB. */
struct nested_ept_atomic_result nested_ept_atomic_walk_wb_ipat_2m(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned maxphys,
    const struct nested_ept_atomic_ops *,void *,struct ad_entry_stamp,
    struct nested_ept_atomic_trace *);
/* Explicit N45 opt-in additionally allows immutable 1 GiB PDPTE leaves.
 * Full 30-bit source offset precedes per-4KiB backing authorization. No host
 * contiguity or complete-range coverage is implied. All old APIs stay strict;
 * source mutation, arbitrary types and native huge EPT02 remain unsupported. */
struct nested_ept_atomic_result nested_ept_atomic_walk_wb_ipat_1g(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned maxphys,
    const struct nested_ept_atomic_ops *,void *,struct ad_entry_stamp,
    struct nested_ept_atomic_trace *);
/* N46A separate fixed UC-data opt-in. The typed resolver attests the exact
 * current host/EPT01 type of dedicated nonexecuting UC data, or ordinary WB
 * backing. UC code, tables and paging structures are forbidden. It must retain
 * that audited type and backing through every source/cache consumer lifetime.
 * EPTP/source tables remain WB, IPAT1 required, 4K only; no effective-type
 * combination table or live retyping is inferred by this API. */
typedef bool (*nea_typed_leaf_fn)(void *,uint64_t,uint64_t *,uint8_t *,uint8_t *);
struct nested_ept_atomic_result nested_ept_atomic_walk_uc_ipat(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned maxphys,
    const struct nested_ept_atomic_ops *,nea_typed_leaf_fn,void *,struct ad_entry_stamp,
    struct nested_ept_atomic_trace *);
#endif
