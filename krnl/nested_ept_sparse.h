#ifndef SYLE_NESTED_EPT_SPARSE_H
#define SYLE_NESTED_EPT_SPARSE_H
#include "nested_ept.h"
struct nested_sparse_page { uint64_t hpa; void *view; };
struct nested_sparse_ops {
    bool (*alloc)(void *context,struct nested_sparse_page *page);
    void (*free)(void *context,struct nested_sparse_page page);
    bool (*flush)(void *context,uint64_t eptp);
};
/* Trusted immutable lease binding: admission must validate owned HPA and
 * pinned epoch/current cache state, not merely trust EPT12 type fields.
 * Called for all typed-mode mappings on fill and cache use; cannot mutate
 * or re-enter this tree. Bindings cannot change while leaves remain cached. */
typedef bool (*nested_sparse_typed_admit_fn)(void *context,const struct nested_ept_result *resolved);
struct nested_sparse_node;
struct nested_sparse_leaf;
struct nested_ept_sparse {
    struct nested_sparse_ops ops;
    void *context;
    struct nested_sparse_node *root,*nodes;
    struct nested_sparse_leaf *leaves;
    uint64_t eptp,generation,fill_count;
    unsigned maxphys;
    bool poisoned,hardware_ad;
    nested_sparse_typed_admit_fn typed_admit;
    void *typed_context;
    bool typed_atomic_uc; /* only explicit fixed-UC AD1 admission setter */
    bool typed_allow_pat,typed_allow_wt;
};
enum nested_sparse_fill {
    NESTED_SPARSE_OK, NESTED_SPARSE_INVALID, NESTED_SPARSE_OOM,
    NESTED_SPARSE_EXISTS, NESTED_SPARSE_POISONED, NESTED_SPARSE_FLUSH_FAILED
};
enum nested_sparse_lookup {
    NESTED_SPARSE_HIT, NESTED_SPARSE_MISS,
    NESTED_SPARSE_BAD_LOOKUP, NESTED_SPARSE_LOOKUP_POISONED
};
/* Trusted allocator contract: every successful allocation owns a distinct
 * live 4K RAM page, with a unique HPA and nonaliasing aligned CPU view.
 * free cannot fail. Failed alloc transfers no ownership. No allocator page
 * may alias external guest/backing pages or another tree's pages.
 * The core validates alignment/width; uniqueness is the allocator's duty.
 * Caller serializes all operations and stops guest execution during edits.
 * init requires a fresh/uninitialized object; reinitializing a live tree is
 * forbidden. There are no fixed GPA windows or node-count arrays. */
/* init flushes the empty EPTP to invalidate any prior use of its root HPA.
 * On flush failure returns false with poisoned owned pages: call destroy
 * to retry invalidation before freeing. Failed init must not be abandoned. */
bool nested_ept_sparse_init(struct nested_ept_sparse *tree,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys);
/* Caller gates hardware EPT A/D capability before selecting this mode. */
bool nested_ept_sparse_init_mode(struct nested_ept_sparse *tree,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys,bool hardware_ad);
/* One-time opt-in, fresh generation1 with no leaf ever installed. Callback
 * stays configured across resets; refresh its lease bindings only after a
 * successful reset removed all leaves. Explicit WB walk, IPAT1, WB/UC leaves
 * only. UC requires hardware A/D off and cannot use write tracking. */
/* Separate fixed UC AD1 opt-in; legacy setters remain WB-only under AD1. */
bool nested_ept_sparse_set_typed_admission_atomic_uc(struct nested_ept_sparse *,nested_sparse_typed_admit_fn,void *);
bool nested_ept_sparse_set_typed_admission_wt(struct nested_ept_sparse *,nested_sparse_typed_admit_fn,void *);
bool nested_ept_sparse_set_typed_admission_pat(struct nested_ept_sparse *,nested_sparse_typed_admit_fn,void *);
bool nested_ept_sparse_set_typed_admission(struct nested_ept_sparse *tree,
    nested_sparse_typed_admit_fn admit,void *context);
enum nested_sparse_fill nested_ept_sparse_fill_tracked(struct nested_ept_sparse *tree,
    uint64_t page_gpa,const struct nested_ept_result *resolved,bool trap_write);
/* These validate the owned hardware path; false leaves query output unchanged.
 * Promote requires a tracked writable mapping. Flush failure poisons/retains
 * all ownership and tracking. Caller stops guest execution during edits. */
bool nested_ept_sparse_query_tracking(const struct nested_ept_sparse *tree,
    uint64_t exact_gpa,bool *tracked);
bool nested_ept_sparse_promote_write(struct nested_ept_sparse *tree,uint64_t exact_gpa);
enum nested_sparse_fill nested_ept_sparse_fill(struct nested_ept_sparse *tree,
    uint64_t page_gpa,const struct nested_ept_result *resolved);
/* Narrow a successful read-walk to an actual access mask. Existing L0
 * backing failures/malformed results remain failures, never L1 denials. */
struct nested_ept_result nested_sparse_walk_access(struct nested_ept_result result,uint8_t access);
/* Cached metadata plus owned EPT02 path consistency check: no EPT12 walk
 * or external resolver during lookup. A tampered path returns BAD_LOOKUP.
 * access is a nonzero mask 1..7; EVERY requested bit must be permitted.
 * One address only: a range spanning pages requires separate lookups. */
enum nested_sparse_lookup nested_ept_sparse_lookup(const struct nested_ept_sparse *tree,
    uint64_t exact_gpa,uint8_t access,struct nested_ept_result *result);
/* Root/EPTP stable across reset. Epoch starts at 1 and changes only on
 * successful reset; fill_count counts successful fills in the current epoch.
 * Flush failure poisons and retains all ownership. Poison cannot reset/fill;
 * only destroy may retry flushing. Caller clears hardware VMCS references
 * before destroy. No freeing on failed destroy. */
bool nested_ept_sparse_reset(struct nested_ept_sparse *tree);
/* Called after successful local flush while all nodes remain owned. A false
 * receipt commit poisons/retains the entire tree; retry requires another flush.
 * Caller has stopped execution and revoked every refill/VMCS reference. */
typedef bool (*nested_sparse_retire_fn)(void *context,const struct nested_ept_sparse *);
/* Exact receipt after root clear and flush, before freeing anything or
 * changing generation. Use this API when lower-page retirement requires a
 * post-flush receipt; a context transaction reset notification is too late.
 * Success frees lower nodes/leaves, retains root/EPTP, then advances generation. */
bool nested_ept_sparse_reset_guarded(struct nested_ept_sparse *,nested_sparse_retire_fn,void *);
bool nested_ept_sparse_destroy_guarded(struct nested_ept_sparse *,nested_sparse_retire_fn,void *);
bool nested_ept_sparse_destroy(struct nested_ept_sparse *tree);
#endif
