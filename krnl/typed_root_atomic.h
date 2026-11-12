#ifndef SYLE_TYPED_ROOT_ATOMIC_H
#define SYLE_TYPED_ROOT_ATOMIC_H
#include "typed_root_registry.h"
#include "nested_atomic_catalog.h"

/* N41B owner-local production registry bridge. One stopped physical consumer,
 * one writer and stable storage per owner; distinct owners share only catalog,
 * updater gate, typed directory/lease and their short metadata lock. This is
 * not a VMX implementation, source replacement or backing-release authority. */
struct tr_atomic_binding {
 struct tr_registry_slot *slot;
 struct nrc_root root;                 /* immutable creation-time identity */
 struct nrc_session pin;                /* persistent conservative source pin */
 uint64_t pin_nonce;                    /* independent saved ticket */
 bool bound,released;
};
struct tr_atomic_trace {
 struct tr_atomic_binding *binding;
 struct nac_trace trace;
 bool used,registered,usable;
};
struct tr_atomic_owner;
struct tr_atomic_maintenance;
struct tr_source_replace;
struct tr_atomic_fault {
 struct tr_atomic_owner *owner;
 struct tr_atomic_binding *binding;
 struct nested_ept_context_item *item;
 struct td_operation operation;
 uint64_t nonce,type_epoch,virtual_key,gpa;
 struct td_identity identity;
 struct trl_token type_token;
 struct nac_cache_identity cache;
 struct nrc_root source;
 struct nac_permit permit;
 struct nac_ticket ticket;              /* independent saved ticket */
 struct tr_atomic_trace *record;
 struct nac_trace fresh;
 bool active,admitted,transient,fatal;
};
struct tr_atomic_config {
 struct nested_root_catalog *catalog;
 struct nrc_identity catalog_identity;
 void *authority_context;
 struct ad_updater_gate *gate;
 struct ad_entry_stamp stamp;           /* independent of type/cache epochs */
 struct tr_registry_slot *slots;
 struct tr_atomic_binding *bindings;
 size_t slot_count;
 struct tr_atomic_trace *traces;
 size_t trace_count;                    /* finite, not recycled in this stage */
 void (*lock)(void *),(*unlock)(void *);
 unsigned (*current_cpu)(void *);
 bool (*consumer_stopped)(void *);
 void *context;
};
struct tr_atomic_owner {
 struct tr_source_replace *replacement; /* separate retained-source protocol */
 struct tr_atomic_maintenance *maintenance; /* optional fresh N42A binding */
 struct typed_root_registry *bridge;
 struct nested_ept_contexts *registry;
 struct tr_atomic_config config;
 struct tr_atomic_fault fault;          /* never stack-based or moved */
 /* Stable entry admission survives a failed td_end. Never stack storage. */
 struct td_operation entry_operation;
 uint64_t entry_nonce;
 bool entry_active;
 bool initialized,poisoned;
};
enum tr_atomic_status {
 TR_ATOMIC_FILLED,TR_ATOMIC_PROMOTED,TR_ATOMIC_DENIED,
 TR_ATOMIC_REJECTED,TR_ATOMIC_FATAL
};
/* Fresh owner/bridge/AD1 registry and zeroed exclusive binding/trace storage.
 * Provider reserves only config.slots with never-reused allocation lifetimes.
 * Installs transactions/guards; never replaces existing hooks. The trusted
 * consumer callbacks attest current CPU and actual stopped VMX consumer.
 * No source callback/operation, sparse allocation or INVEPT runs under
 * config.lock. Admission/provider guards and reserve are metadata-only
 * nonallocating callbacks under that lock. */
bool tr_atomic_init(struct tr_atomic_owner *,struct typed_root_registry *,
 struct nested_ept_contexts *,struct typed_root_lease *,uint64_t owner,
 const struct tr_registry_root_provider *,void *provider_context,
 const struct tr_atomic_config *);
/* Fault means a real local cache miss or tracked first write, never a routine
 * HIT. gpa is normalized to a page. Source/updater/type/transient refill permits
 * span lookup, source guard/CAS, persistent trace registration, physical sparse
 * fill/promotion, actual flush and final guard. A read-created writable mapping
 * is always installed RO, regardless of a peer's shared D. */
enum tr_atomic_status tr_atomic_fault(struct tr_atomic_owner *,
 struct nested_ept_context_item *,uint64_t gpa,uint8_t access,
 struct nested_ept_result *translation_out);
/* Every actual VMCS02 entry, including internal resumes. Caller first proves
 * physical VMCS identity, VMREADs its complete EPTP and supplies the actual
 * pinned hardware item. Records residency before execution. No guest entry
 * occurs here. Failed cleanup retains entry_operation and poisons this owner. */
bool tr_atomic_ready(struct tr_atomic_owner *);
bool tr_atomic_entry(struct tr_atomic_owner *,struct nested_ept_context_item *,uint64_t actual_eptp);
/* Cleanup only, not publication/reentry authority. Retains each pointer/flag
 * until its own release succeeds. May retry a retained NAC cleanup with the
 * original tickets; poisoned directory registrations conservatively remain. */
bool tr_atomic_fault_cleanup(struct tr_atomic_owner *);
#endif
