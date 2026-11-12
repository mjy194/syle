#ifndef SYLE_CACHE_LEASE_REFS_H
#define SYLE_CACHE_LEASE_REFS_H
#include <stdbool.h>
#include <stdint.h>
struct cache_lease_refs;
/* Caller owns storage (e.g. context attachment). Must be zero-initialized,
 * stable, and not copied/mutated while linked. One token per unique owner.
 * Token contains no page ownership proof; it pins an externally proved lease. */
struct cache_lease_token {
    struct cache_lease_refs *ledger;
    struct cache_lease_token *next;
    const void *owner;
    uint64_t generation;
    bool linked;
};
enum cache_lease_refs_phase { CACHE_LEASE_REFS_ACTIVE, CACHE_LEASE_REFS_POISON };
struct cache_lease_refs {
    struct cache_lease_token *head;
    uint64_t generation,refs;
    enum cache_lease_refs_phase phase;
};
/* Serialized caller only: failure-atomic bookkeeping, NOT concurrent atomic
 * synchronization. Fresh objects only; init is not poison recovery. No heap
 * allocation or fixed context capacity: caller supplies each token's storage.
 * Failure to obtain that storage must happen before calling acquire. */
bool cache_lease_refs_init(struct cache_lease_refs *,uint64_t generation);
bool cache_lease_refs_acquire(struct cache_lease_refs *,struct cache_lease_token *,
                              const void *owner,uint64_t generation);
bool cache_lease_refs_admit(const struct cache_lease_refs *,const struct cache_lease_token *,
                            const void *owner,uint64_t generation);
/* Caller may release ONLY after safe context destroy/VMCS reference removal
 * and required invalidation. Tree reset does NOT release its token.
 * Poisoned ledgers permit verified safe releases, never new admission. */
bool cache_lease_refs_release(struct cache_lease_refs *,struct cache_lease_token *,
                              const void *owner,uint64_t generation);
/* Advance only after all references released; no implicit hardware/cache work. */
bool cache_lease_refs_advance(struct cache_lease_refs *,uint64_t expected_generation,
                              uint64_t new_generation);
void cache_lease_refs_poison(struct cache_lease_refs *);
#endif
