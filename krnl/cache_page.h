#ifndef SYLE_CACHE_PAGE_H
#define SYLE_CACHE_PAGE_H
#include <stdbool.h>
#include <stdint.h>
/* Trusted resolver supplies a whole stable readable 4KiB table; no casting
 * physical addresses. Caller excludes LA57 and guarantees NXE enabled. */
typedef bool (*cache_page_read_fn)(void *ctx,uint64_t table_hpa,const uint64_t **view);
enum cache_page_audit_status {
    CACHE_PAGE_AUDIT_OK, CACHE_PAGE_BAD_INPUT, CACHE_PAGE_UNREADABLE,
    CACHE_PAGE_UNSUPPORTED, CACHE_PAGE_CYCLE, CACHE_PAGE_BUDGET,
    CACHE_PAGE_TABLE_ALIAS, CACHE_PAGE_HUGE_ALIAS, CACHE_PAGE_ALIAS,
    CACHE_PAGE_NOT_MAPPED, CACHE_PAGE_ATTRIBUTES
};
struct cache_page_lease {
    uint64_t root_hpa,target_hpa,table_hpa,oldraw;
    unsigned index,table_visits;
    bool audited;
};
/* max_table_visits bounds traversal, not the address space. Shared subtrees
 * are traversed separately for each VA path, so aliases cannot hide.
 * Requires exactly one 4K identity mapping, effective RW/NX, PAT index0 WB, and reserved transition PAT index3 UC.
 * This only audits page-table mappings at a frozen instant, NOT PMM ownership,
 * MTRRs, DMA, aliases in other CR3s or device accesses. Caller proves those.
 * On failure out is unchanged; no page tables are written. */
enum cache_page_audit_status cache_page_audit(uint64_t root_hpa,uint64_t target_hpa,
    uint64_t pat,unsigned max_table_visits,cache_page_read_fn read,void *ctx,
    struct cache_page_lease *out);
/* Read-only WB-only full-tree uniqueness/role audit. Requires exactly one
 * low (<4GiB) 4K identity mapping, effective RW/NX with leaf RW/NX, PAT index0
 * WB, ordinary table-walk cache bits, and all eight PAT bytes legal. No UC or
 * WT slot is required. This does not authorize a cache-type transition or
 * prove MTRRs, ownership, DMA exclusion, or mappings in another CR3. Caller
 * supplies the same frozen readable-table/NXE/four-level proof as above.
 * Failure leaves out unchanged; never writes, frees, or changes mappings. */
enum cache_page_audit_status cache_page_audit_wb(uint64_t root_hpa,uint64_t target_hpa,
    uint64_t pat,unsigned max_table_visits,cache_page_read_fn read,void *ctx,
    struct cache_page_lease *out);
/* Same full-tree uniqueness/role audit for the CURRENT target mapping.
 * Only WB(6) at PAT0 or UC(0) at PAT3, with PAT0 WB/PAT3 UC unchanged.
 * Not a cache transition or fresh MTRR proof; caller retains initial proof. */
enum cache_page_audit_status cache_page_audit_current(uint64_t root_hpa,uint64_t target_hpa,
    uint64_t pat,unsigned current_type,unsigned max_table_visits,
    cache_page_read_fn read,void *ctx,struct cache_page_lease *out);
bool cache_page_lease_matches(const struct cache_page_lease *,uint64_t current_raw);
/* Address/permission/cache identity exact, tolerating hardware A/D only. */
/* Independently select an exact UC(0) or WT(4) PAT slot. All bytes must be
 * legal PAT types and slot0 must remain WB. Lowest matching slot wins.
 * 4K PTE encoding only (PWT bit3, PCD bit4, PAT bit7, NOT huge-page bit12).
 * Failure leaves both outputs unchanged. The legacy audit still requires
 * slot3 UC; WT availability is checked separately by this helper. */
bool cache_page_pat_slot(uint64_t pat,uint8_t requested,unsigned *slot,uint64_t *pte_bits);
enum cache_page_phase { CACHE_PAGE_WB, CACHE_PAGE_TRANSITION,CACHE_PAGE_UC,CACHE_PAGE_POISON,CACHE_PAGE_WT };
struct cache_page_proof {
    bool owned_private_ram,single_cpu,guest_quiescent,no_dma,frozen_mappings;
};
enum cache_page_step {
    CACHE_PAGE_DETACH_GUEST_INVEPT,
    CACHE_PAGE_HOST_NONPRESENT,
    CACHE_PAGE_TLB_BEFORE,
    CACHE_PAGE_CACHE_BEFORE,
    CACHE_PAGE_INSTALL,
    CACHE_PAGE_TLB_AFTER,
    CACHE_PAGE_CACHE_AFTER,
    CACHE_PAGE_STEPS
};
struct cache_page_registry {
    struct cache_page_lease lease;
    enum cache_page_phase phase,destination;
    uint64_t generation;
    unsigned next_step;
};
/* Fresh registry only. Proofs are external caller attestations, not inferred
 * from PTE bits. This bookkeeping executes no architectural cache protocol. */
bool cache_page_init(struct cache_page_registry *,const struct cache_page_lease *,
                     const struct cache_page_proof *);
bool cache_page_begin(struct cache_page_registry *,enum cache_page_phase destination,
                      const struct cache_page_proof *,uint64_t *ticket);
/* Record successful completion in exactly the listed order. These are caller
 * attestations, NOT proof that instructions ran. Any partial/uncertain action
 * requires fail(), never a speculative acknowledgement or rollback. */
bool cache_page_ack(struct cache_page_registry *,uint64_t ticket,enum cache_page_step);
bool cache_page_commit(struct cache_page_registry *,uint64_t ticket);
void cache_page_fail(struct cache_page_registry *);
static inline bool cache_uc_vmx_capable(uint64_t primary,uint64_t secondary,uint64_t ept) {
    const uint64_t need=(UINT64_C(1)<<6)|(UINT64_C(1)<<14)|(UINT64_C(1)<<20)|(UINT64_C(1)<<25);
    return (primary&(UINT64_C(1)<<63))&&(secondary&(UINT64_C(1)<<33))&&(ept&need)==need;
}

/* Explicit WB/WT profile. slot/bits must equal the lowest existing WT PAT
 * selection from a fully legal PAT with slot0 WB. WT audit requires that
 * exact 4K PTE encoding, including high-slot PAT bit7. WB audit requires slot0.
 * Does not require UC at slot3; old WB/UC APIs retain that separate contract.
 * Failure leaves out unchanged. No PAT mutation or MTRR proof here. */
enum cache_page_audit_status cache_page_audit_current_type(uint64_t root,uint64_t target,uint64_t pat,
    unsigned type,unsigned slot,uint64_t bits,unsigned budget,cache_page_read_fn read,void *ctx,struct cache_page_lease *out);

#endif
