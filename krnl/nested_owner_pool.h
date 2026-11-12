#ifndef SYLE_NESTED_OWNER_POOL_H
#define SYLE_NESTED_OWNER_POOL_H
#include "nested_ept_sparse.h"
#include <stddef.h>
struct nested_owner_slot { uint64_t generation; bool live; };
struct nested_owner_token {
    uint64_t owner,incarnation,generation;
    size_t slot;
    struct nested_sparse_page page;
};
struct nested_owner_pool {
    uint64_t owner,incarnation;
    const struct nested_sparse_page *pages;
    struct nested_owner_slot *slots;
    size_t count,live;
    unsigned maxphys;
    bool active,fault;
};
/* Fresh init only. BSP owns immutable pages[] and disjoint slots[] storage
 * throughout lifetime. No PMM, zeroing, flush, locking or CPU identity lookup.
 * Caller guarantees private owned RAM/WB and no aliases outside this list;
 * pointer equality/alignment cannot prove physical ownership. Serialize calls.
 * Owner zero is valid (e.g. APIC ID 0); incarnation must be nonzero.
 * An incarnation must never be reused for the same owner after detach. */
bool nested_owner_pool_init(struct nested_owner_pool *,uint64_t owner,uint64_t incarnation,
    const struct nested_sparse_page *,struct nested_owner_slot *,size_t count,unsigned maxphys);
bool nested_owner_pool_take(struct nested_owner_pool *,uint64_t owner,uint64_t incarnation,
    struct nested_owner_token *out);
bool nested_owner_pool_give(struct nested_owner_pool *,struct nested_owner_token);
/* Detach only; BSP frees backing afterward. quiescent is caller attestation
 * that hardware references/invalidation are complete, not a proof by this API. */
bool nested_owner_pool_release_all(struct nested_owner_pool *,uint64_t owner,uint64_t incarnation,bool quiescent);
/* Sparse adapter cannot carry generation. Requires synchronous allocation /
 * single free per loan, no copied or delayed old frees across reuse. Strong
 * token API above rejects stale generations; this tuple-only API cannot.
 * Invalid free latches fault and retains ownership; valid give remains legal. */
bool nested_owner_pool_alloc(void *context,struct nested_sparse_page *out);
void nested_owner_pool_free(void *context,struct nested_sparse_page page);
#endif
