#ifndef SYLE_NESTED_EPT_CONTEXTS_H
#define SYLE_NESTED_EPT_CONTEXTS_H
#include "nested_ept_sparse.h"
#include "cache_lease_refs.h"
#define NESTED_EPT_CONTEXT_DEFAULT_BUDGET 4u
enum nested_context_event {
    NESTED_CONTEXT_INIT, NESTED_CONTEXT_RESET, NESTED_CONTEXT_DESTROY
};
struct nested_ept_context_item;
typedef void (*nested_context_lifecycle_fn)(void *context,
    struct nested_ept_context_item *item,enum nested_context_event event);
enum nested_context_operation { NESTED_CONTEXT_SELECT, NESTED_CONTEXT_INVALIDATE, NESTED_CONTEXT_DISPOSE, NESTED_CONTEXT_PIN, NESTED_CONTEXT_UNPIN, NESTED_CONTEXT_CLEAR_PINS };
/* Opt-in, serialized, nonreentrant transaction bridge. begin/end encompass
 * offline candidates too. reserve must transfer a stable nonnull token on
 * success and no ownership on failure. cancel consumes a no-root reservation
 * on success; on failure the bridge retains that reservation. attach sees
 * root-owning initial-flush failures with init_ok=false. publish must commit
 * directory membership/refill authorization before reporting true. retire is
 * called AFTER a real flush but BEFORE any node is freed; it must durably
 * record receipts covering all historical consumers, or return false. This
 * registry only performs its own local flush; other receipts are external.
 * No callback may run guest code or reenter the registry. end cannot fail.
 * Direct sparse fill/promote calls require a separate external refill permit;
 * these wrappers cannot gate callers using a tree pointer directly.
 * retain is nonfailing/no-allocation: reserved ledger storage must suffice to
 * record a root-owning failed candidate, including an unsuccessful attach.
 * All mandatory callbacks are validated at installation. */
struct nested_context_transactions {
 bool (*begin)(void *,enum nested_context_operation);
 void (*end)(void *,enum nested_context_operation);
 bool (*reserve)(void *,uint64_t,void **);
 bool (*cancel)(void *,void *);
 bool (*attach)(void *,void *,struct nested_ept_context_item *,bool init_ok);
 void (*retain)(void *,void *,struct nested_ept_context_item *);
 bool (*publish)(void *,void *,struct nested_ept_context_item *);
 bool (*reset)(void *,void *,struct nested_ept_context_item *);
 bool (*retire)(void *,void *,struct nested_ept_context_item *);
};
/* Separate fresh-only extension: the positional transaction ABI above is
 * unchanged. Guards are fallible, run within the admitted transaction, and
 * must finish before selection/LRU mutation or sparse reset/destruction.
 * They may revoke external traces; failure poisons and retains the item. */
struct nested_context_guards {
 bool (*select)(void *,struct nested_ept_context_item *);
 bool (*pre_reset)(void *,struct nested_ept_context_item *);
 bool (*pre_retire)(void *,struct nested_ept_context_item *);
};
struct nested_ept_context_item {
    struct nested_sparse_page own_page;
    uint64_t key,last_used;
    uint32_t pins;
    _Alignas(16) union { uint64_t q[16]; struct cache_lease_token uc_token; } attachment;
    bool attachment_initialized;
    void *transaction_token;
    struct nested_ept_sparse tree;
    struct nested_ept_context_item *next;
};
_Static_assert(sizeof(((struct nested_ept_context_item *)0)->attachment)==128,"attachment ABI remains 128 bytes");
struct nested_ept_contexts {
    struct nested_sparse_ops ops;
    void *allocator_context;
    unsigned maxphys,max_resident,count;
    uint64_t tick;
    bool evict,initialized,poisoned,hardware_ad,mixed_ad;
    nested_context_lifecycle_fn lifecycle;
    void *hook_context;
    const struct nested_context_transactions *transactions;
    void *transaction_context;
    const struct nested_context_guards *guards;
    void *guard_context;
    struct nested_ept_context_item *items,*active;
};
enum nested_context_status {
    NESTED_CONTEXT_OK, NESTED_CONTEXT_INVALID, NESTED_CONTEXT_LIMIT,
    NESTED_CONTEXT_OOM, NESTED_CONTEXT_POISONED, NESTED_CONTEXT_FLUSH_FAILED,
    NESTED_CONTEXT_OVERFLOW
};
/* Fresh object only. All metadata and EPT nodes share a trusted allocator of
 * unique live pages. Caller serializes operations/stops guest execution.
 * Default is resource budget 4, no eviction; no fixed slot/address array. */
bool nested_ept_contexts_init(struct nested_ept_contexts *,
    const struct nested_sparse_ops *,void *allocator_context,unsigned maxphys);
bool nested_ept_contexts_init_budget(struct nested_ept_contexts *,
    const struct nested_sparse_ops *,void *allocator_context,unsigned maxphys,
    unsigned max_resident,bool evict);
/* Configured AD mode binds all keys to exactly low 0x5e (AD1) or 0x1e
 * (AD0). Caller gates hardware AD support. Lifecycle callbacks are trusted,
 * nonfailing, nonreentrant, and may modify only attachment storage/resources.
 * INIT runs after sparse init succeeds, before candidate linkage/selection;
 * RESET only after successful sparse reset; DESTROY after successful sparse
 * destroy and before item metadata free. Failed invalidations retain storage.
 * A candidate whose initial flush failed gets no INIT/DESTROY callback. */
bool nested_ept_contexts_init_config(struct nested_ept_contexts *,
    const struct nested_sparse_ops *,void *allocator_context,unsigned maxphys,
    unsigned max_resident,bool evict,bool hardware_ad,
    nested_context_lifecycle_fn lifecycle,void *hook_context);
/* Explicit mixed-mode registry: full keys 0x...1e and 0x...5e remain
 * distinct caches even for the same EPT12 root. Each tree's physical EPTP AD
 * mode follows its key. Caller gates hardware AD capability before using
 * this API. Type1 invalidation matches root and resets every mode alias.
 * Existing init/config APIs continue to enforce their single fixed mode. */
bool nested_ept_contexts_init_mixed(struct nested_ept_contexts *,
    const struct nested_sparse_ops *,void *allocator_context,unsigned maxphys,
    unsigned max_resident,bool evict,nested_context_lifecycle_fn,void *hook_context);
/* Install only on a fresh empty initialized registry. Ops must outlive it.
 * Old void lifecycle hooks retain their existing ordering/contract. */
bool nested_ept_contexts_set_transactions(struct nested_ept_contexts *,
 const struct nested_context_transactions *,void *);
bool nested_ept_contexts_set_guards(struct nested_ept_contexts *,
 const struct nested_context_guards *,void *);
enum nested_context_status nested_ept_contexts_select(struct nested_ept_contexts *,uint64_t full_eptp12);
struct nested_ept_sparse *nested_ept_contexts_active(struct nested_ept_contexts *);
enum nested_sparse_lookup nested_ept_contexts_lookup(struct nested_ept_contexts *,
    uint64_t gpa,uint8_t access,struct nested_ept_result *result);
/* A pin protects a VMCS reference, not merely the current software selection.
 * Pin new item BEFORE VMWRITE. Unpin old only AFTER successful switch. If
 * switch outcome is uncertain, retain both pins until VMCS is cleared. */
struct nested_ept_context_item *nested_ept_contexts_pin_active(struct nested_ept_contexts *);
bool nested_ept_contexts_unpin(struct nested_ept_contexts *,struct nested_ept_context_item *);
/* Explicit caller attestation: ALL hardware VMCS references are CLEAR.
 * Also callable while poisoned, to permit safe final cleanup. */
void nested_ept_contexts_release_all_after_clear(struct nested_ept_contexts *);
/* Type1 validates full EPTP then matches root bits 51:12; uncached root no-op.
 * Type2 ignores key. Descriptor reserved-word validation is external.
 * Partial reset failure poisons the registry, without claiming rollback.
 * Reset retains roots, so pins do not prevent invalidation while CPUs stopped. */
enum nested_context_status nested_ept_contexts_invalidate(struct nested_ept_contexts *,
    uint64_t type,uint64_t full_eptp12);
/* Any pin rejects the entire destroy without mutation/freeing. Once unpinned,
 * attempt every context; flush failures retain item+pages for retry.
 * No operational calls except pin-release/destroy may proceed after poison. */
bool nested_ept_contexts_destroy(struct nested_ept_contexts *);
#endif
