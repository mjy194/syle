#ifndef SYLE_NESTED_OWNER_AD_H
#define SYLE_NESTED_OWNER_AD_H
#include "nested_owner_sparse.h"
#include "nested_ept_atomic.h"
#include "ad_updater_gate.h"
struct nested_owner_ad_record;
struct nested_owner_ad {
    struct nested_owner_sparse *owner;
    struct ad_updater_gate *gate;
    struct ad_entry_stamp stamp;
    struct ad_updater_token token;
    uint64_t nonce;
    struct nested_owner_ad_record *records;
    uint64_t readwalks,promotions,target_promotions;
    uint64_t reset_generation;
    bool reset_pending;
    bool authority,bound,failed;
};
/* Fresh BSP-owned object. Owner tree must already be initialized AD1, selected
 * EPT12=1005e, no multiple-context mode. Caller proves all guest table aliases
 * structurally read-only, pinned WB, and whole lifetime ownership. BSP sets
 * gate/stamp/authority only after this complete domain proof; a bare boolean
 * from an untrusted source is NOT authority. Owner calls bind after tree init. */
bool nested_owner_ad_bind(struct nested_owner_ad *,struct nested_owner_sparse *);
/* Local owner only, stopped guest. Real lease acquired for validation/CAS/fill.
 * Any failure poisons and retains records. Normal leases are released; failed
 * releases retain the stable token. No retry/free on failed domains. */
bool nested_owner_ad_fault(struct nested_owner_ad *,uint64_t gpa,uint8_t need);
/* Successful local owner path only, after actual clear/invalidation, no
 * remaining EPT02 root (tree destroy succeeded), and no live local lease.
 * Other owners may still run: these records are owner-private, not shared
 * table backing. BSP must separately close/drain before shared backing free.
 * Caller attests quiescence; no hardware action here. */
bool nested_owner_ad_release_records(struct nested_owner_ad *,bool quiescent);
/* Local owner VMX-root only, guest stopped; caller attests root_quiescent.
 * Shared updater gate must already be closed and drained at the full stamp.
 * Reset keeps EPTP/root and executes sparse flush; records are freed ONLY
 * after success. Flush failure poisons/retains. Cumulative stats remain. */
bool nested_owner_ad_local_reset(struct nested_owner_ad *,bool root_quiescent);
/* Local owner, after reset and before gate reopen. Only epoch may advance by
 * one; gate is still closed/drained at old. No entry allowed until global
 * reopen validates the same new stamp. Does not reopen gate or clear A/D. */
bool nested_owner_ad_advance_stamp(struct nested_owner_ad *,
    struct ad_entry_stamp old_stamp,struct ad_entry_stamp new_stamp);
#endif
