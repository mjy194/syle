#ifndef SYLE_NESTED_EPT_ATOMIC_LEASE_H
#define SYLE_NESTED_EPT_ATOMIC_LEASE_H
#include "nested_ept_atomic.h"
#include "ad_updater_gate.h"
/* Caller-owned stable session storage survives any failed release. Do not
 * discard/reuse it or free its tables until released=true. The gate accounts
 * updater lifetime, NOT guest consumer entry or a physical memory pin. */
struct nested_ept_atomic_session {
    struct ad_updater_token token;
    uint64_t nonce;
    bool acquired,released;
};
/* Acquires the real gate lease before any resolver/load. Composes its guard
 * with the trusted backend's ownership guard, and releases after the complete
 * walk+CAS. Failed release poisons the gate and retains the caller's token.
 * No callbacks/metadata lock extend past return, but retained pin may do so. */
struct nested_ept_atomic_result nested_ept_atomic_walk_leased(
    struct ad_updater_gate *,struct nested_ept_atomic_session *,
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned maxphys,
    const struct nested_ept_atomic_ops *,void *,struct ad_entry_stamp,
    struct nested_ept_atomic_trace *);
#endif
