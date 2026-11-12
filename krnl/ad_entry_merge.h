#ifndef SYLE_AD_ENTRY_MERGE_H
#define SYLE_AD_ENTRY_MERGE_H
#include <stdbool.h>
#include <stdint.h>
#define AD_ENTRY_A UINT64_C(0x100)
#define AD_ENTRY_D UINT64_C(0x200)
struct ad_entry_stamp { uint64_t domain_id,incarnation,context_id,epoch,lifetime; };
/* Pure injected backend. No actual pointer, HPA or atomic instruction is
 * supplied by this module. Backend guarantees a pinned, aligned, writable WB
 * 64-bit entry for the WHOLE call, including after guard failure. guard checks
 * ownership/epoch/lifetime admission; it is NOT the lifetime pin itself.
 * load and compare_exchange must be linearizable. CAS false is an ordinary
 * failed comparison (including permitted spurious failure), never an unknown
 * hardware write outcome. Side effects/errors outside this contract require
 * caller fail-stop; this model cannot infer hardware ownership or safety. */
struct ad_entry_backend {
    bool (*guard)(void *,struct ad_entry_stamp);
    uint64_t (*load)(void *);
    bool (*compare_exchange)(void *,uint64_t expected,uint64_t desired);
};
struct ad_entry_request {
    struct ad_entry_stamp stamp;
    uint64_t expected_identity,mutable_ad,requested_bits;
    unsigned attempts;
};
enum ad_entry_status { AD_ENTRY_DONE,AD_ENTRY_INVALID,AD_ENTRY_REWALK,
                      AD_ENTRY_STALE,AD_ENTRY_APPLIED_BUT_STALE,AD_ENTRY_RETRY };
struct ad_entry_result {
    enum ad_entry_status status;
    unsigned comparisons;
    /* True iff this invocation completed a value-changing CAS. This says
     * nothing about other writers. APPLIED_BUT_STALE has changed=true: no rollback.
     * DONE means bits satisfied at one observation, not eternally preserved. */
    bool changed;
};
/* Caller validates present/format/path/permissions and chooses mutable bits:
 * nonleaf A only, leaf A|D. Only this operation's requested bits are ORed,
 * never remembered D from an old trace. Neither validation of EPT format nor
 * Intel failure-exit classification is performed here. No runtime integration. */
struct ad_entry_result ad_entry_merge(const struct ad_entry_backend *,void *,
                                     const struct ad_entry_request *);
#endif
