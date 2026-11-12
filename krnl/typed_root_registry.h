#ifndef SYLE_TYPED_ROOT_REGISTRY_H
#define SYLE_TYPED_ROOT_REGISTRY_H
#include "typed_directory_registry.h"
#include "typed_root_lease.h"

struct typed_root_registry;
/* The provider owns stable storage, separate from all pool/registry pages.
 * Slots are finite and never recycled, including cancellations and receipts. */
struct tr_registry_slot {
 struct tr_registry_slot *next;
 struct typed_root_registry *bridge;
 struct td_registry_slot directory;
 struct trl_binding typed;
 struct td_retire_ticket evidence_ticket;
 uint64_t evidence_version,receipt_version,evidence_acked,evidence_serial;
 uint64_t evidence_operation_nonce;
};
/* Explicit caller attestations after guarded_destroy's local flush. The
 * complete ticket and current metadata version bind the supplied evidence.
 * A local callback is NOT proof of remote history or a hardware operation. */
struct tr_registry_flush_evidence {
 struct td_retire_ticket ticket;
 uint64_t directory_version,acked,flush_serial;
};
struct tr_registry_provider {
 bool (*reserve)(void *,struct tr_registry_slot **,uint64_t *allocator_lifetime);
 bool (*flush_evidence)(void *,struct tr_registry_slot *,struct td_retire_ticket,
                       uint64_t directory_version,struct tr_registry_flush_evidence *);
};
/* N39A opt-in root-local evidence. The callback receives an immutable scope,
 * not authority to inspect mutable metadata without its short lock. An exact
 * history ACK attests actual completed invalidation of this physical EPTP on
 * every historical CPU, including after possible HPA reuse. The operation
 * nonce binds this callback attempt; the retirement nonce binds root lifetime.
 * These are caller attestations, never proof inferred from a local flush. */
struct tr_registry_retire_scope {
 struct td_retire_ticket ticket;
 uint64_t operation_nonce;
};
struct tr_registry_root_evidence {
 struct tr_registry_retire_scope scope;
 uint64_t acked,flush_serial;
};
struct tr_registry_root_provider {
 bool (*reserve)(void *,struct tr_registry_slot **,uint64_t *allocator_lifetime);
 bool (*flush_evidence)(void *,struct tr_registry_slot *,struct tr_registry_retire_scope,
                       struct tr_registry_root_evidence *);
};
struct tr_registry_atomic_hooks {
 /* Called under the short metadata lock: no source callbacks or table access. */
 bool (*admit)(void *,struct tr_registry_slot *,const struct nested_ept_result *);
 /* Provider pre/post guard, also metadata-only: exact retained source binding. */
 bool (*pending)(void *,struct tr_registry_slot *,struct nested_ept_context_item *);
 /* Called unlocked AFTER durable receipt and typed retirement, BEFORE forget
  * and any sparse free. Failure retains the stable slot and owned tree. */
 bool (*before_forget)(void *,struct tr_registry_slot *,struct nested_ept_context_item *);
};
struct typed_root_registry {
 struct td_registry_bridge directory;
 struct typed_root_lease *lease;
 struct tr_registry_provider provider;
 struct tr_registry_root_provider root_provider;
 bool root_scoped,atomic_wb;
 struct tr_registry_atomic_hooks atomic_hooks;
 void *atomic_context;
 void *provider_context;
 struct tr_registry_slot *slots;
 uint64_t backing_hpa;
 struct td_operation *refill_operation;
 struct tr_registry_slot *refill_slot;
 uint64_t refill_nonce;
 /* Optional shared metadata serialization. Hooks must be nonfailing and
  * paired. The bridge never holds them across sparse allocation or flush. */
 void (*metadata_lock)(void *),(*metadata_unlock)(void *);
 void *metadata_context;
};
/* Fresh opt-in wrapper only. Existing bridge, lifecycle hooks and default
 * paths are unchanged. Caller attests a dedicated owned data backing HPA,
 * disjoint from every allocator/table page, and serializes metadata access.
 * Attach installs a one-time sparse admission callback: lifecycle hooks must
 * not replace it. Consumers must check directory.faulted after registry calls.
 * No callback here executes INVEPT or a real memory-type transition. */
bool typed_root_registry_init(struct typed_root_registry *,struct typed_root_lease *,
 uint64_t owner,uint64_t backing_hpa,const struct tr_registry_provider *,void *);
/* New explicit opt-in; legacy init retains its serialized global-version
 * evidence contract. One bridge/pool/registry per owner remains single-writer.
 * Distinct owner bridges share one lease/directory and the same short lock.
 * N39B1 opts into a hardware owner-local provider, still with serial owner
 * rounds. Root retirement never frees backing. */
bool typed_root_registry_init_root_scoped(struct typed_root_registry *,struct typed_root_lease *,
 uint64_t owner,uint64_t backing_hpa,const struct tr_registry_root_provider *,void *);
/* New explicit immutable WB/IPAT1/AD1 profile. Only the registered atomic
 * operation may admit mappings. Legacy lookup/fill/miss helpers reject it. */
bool typed_root_registry_init_atomic_wb(struct typed_root_registry *,struct typed_root_lease *,
 uint64_t owner,uint64_t backing_hpa,const struct tr_registry_root_provider *,void *,
 const struct tr_registry_atomic_hooks *,void *);
/* Separate fixed-UC atomic constructor. */
bool typed_root_registry_init_atomic_uc(struct typed_root_registry *,struct typed_root_lease *,
 uint64_t owner,uint64_t backing_hpa,const struct tr_registry_root_provider *,void *,
 const struct tr_registry_atomic_hooks *,void *);
/* Fresh bridge only, before installing transactions. With hooks installed,
 * callers must NOT hold this lock around bridge callbacks. reserve is a
 * preallocated stable-slot provider under the lock; flush_evidence is outside
 * it and may not directly inspect mutable directory/lease fields unlocked. */
bool typed_root_registry_set_sync(struct typed_root_registry *,void (*lock)(void *),
 void (*unlock)(void *),void *context);
const struct nested_context_transactions *typed_root_registry_transactions(void);
/* Predicate and real transaction admission linearize under the SAME metadata
 * lock. Predicate is metadata-only, nonreentrant, and may not acquire locks. */
bool typed_root_registry_begin_guarded(struct typed_root_registry *,
 enum nested_context_operation,bool (*predicate)(void *),void *);
/* The actual sparse call is enclosed by one directory operation and refill
 * registration. The sparse callback rejects direct fill/lookup calls lacking
 * this permit, stale identity/epoch/type, foreign non-WB backing, PAT0 and A/D.
 * Non-target ordinary resolved mappings remain WB/IPAT1; target mappings must
 * use the current lease type. Every result must come from a trusted owned-RAM
 * resolver: type policy does not prove physical ownership. The lease binds
 * the dedicated backing HPA and lifetime. Caller holds the immutable result
 * stable for this call; no external reusable refill plan survives the call.
 * Cached metadata is tree-owned. External plans require separate durable
 * td_refill_add/drop registration, not just this transient permit.
 * These helpers do not authorize VM entry or prove physical ownership. */
enum nested_sparse_fill typed_root_registry_fill(struct typed_root_registry *,
 struct nested_ept_context_item *,uint64_t gpa,const struct nested_ept_result *);
enum nested_sparse_lookup typed_root_registry_lookup(struct typed_root_registry *,
 struct nested_ept_context_item *,uint64_t gpa,uint8_t access,struct nested_ept_result *);
/* Trusted resolver receives a per-call immutable metadata snapshot. One
 * admitted operation/refill registration spans lookup -> EPT12 walk -> fill.
 * The operation prevents a close/type transition while resolve runs unlocked.
 * A cached HIT is not a successful miss and is never refilled. No reusable
 * permit escapes this API, including any lookup/walk/fill failure. */
struct tr_registry_walk {
 uint64_t virtual_key,type_epoch,backing_hpa;
 unsigned type;
};
typedef bool (*tr_registry_resolve_fn)(void *,const struct tr_registry_walk *,
 uint64_t,uint8_t,uint64_t *,const uint64_t **,uint8_t *);
enum nested_sparse_fill typed_root_registry_miss(struct typed_root_registry *,
 struct nested_ept_context_item *,uint64_t gpa,uint8_t access,
 tr_registry_resolve_fn,void *,const struct nested_ept_policy *);
/* N40 opt-in source interface. The same admitted directory operation spans
 * initial source guard, lookup, translation, final guard and sparse fill.
 * The trusted source must retain a caller-stable whole-walk lifetime permit;
 * it is not replaced by the cache type lease. Old miss ABI remains unchanged.
 * Any post-publication guard failure poisons/retains the tree. translation_out
 * preserves L1-vs-L0-vs-unsupported results; a cache HIT is not a miss success. */
struct tr_registry_source {
 bool (*guard)(void *,const struct tr_registry_walk *);
 struct nested_ept_result (*translate)(void *,const struct tr_registry_walk *,uint64_t,uint8_t);
};
enum nested_sparse_fill typed_root_registry_miss_source(struct typed_root_registry *,
 struct nested_ept_context_item *,uint64_t,uint8_t,const struct tr_registry_source *,
 void *,struct nested_ept_result *translation_out);
#endif
