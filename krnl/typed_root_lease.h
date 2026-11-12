#ifndef SYLE_TYPED_ROOT_LEASE_H
#define SYLE_TYPED_ROOT_LEASE_H
#include "typed_directory.h"

/* N38C-A opt-in, serialized policy only. No allocator, lock, alias scan,
 * hardware operation or ownership proof. All calls/direct metadata reads are
 * serialized by an external lock; a held td_operation spans external work.
 * Existing DTL and registry APIs are intentionally independent. */
#define TRL_UC 0u
#define TRL_WB 6u
struct typed_root_lease;
struct trl_backing {
 uint64_t hpa,allocator_lifetime;
 /* External audit: exclusively owned WB data, never code/table/stack. */
 bool dedicated_data_wb;
};
struct trl_token {
 struct td_identity identity;
 uint64_t nonce,type_epoch;
 unsigned type;
};
enum trl_state { TRL_RESERVED,TRL_CANDIDATE,TRL_ATTACHED,TRL_LIVE,TRL_RETAINED,TRL_RETIRED };
struct trl_binding {
 struct trl_binding *next;
 struct typed_root_lease *lease;
 /* Stable until token retirement/cancellation, then never dereferenced. */
 struct td_record *record;
 struct trl_token token;
 uint64_t owner,allocator_lifetime,constructing_nonce,full_eptp,hpa;
 struct td_retire_ticket retirement;
 const struct td_receipt *receipt;
 uint64_t receipt_version,receipt_flush_serial,prepared_epoch;
 unsigned prepared_type;
 enum trl_state state;
 bool token_live,attached,prepared;
};
struct typed_root_lease {
 struct typed_directory *directory;
 struct trl_backing backing;
 struct trl_binding *bindings;
 uint64_t domain,incarnation,type_epoch,version,next_nonce;
 size_t budget,reserved,live_tokens;
 unsigned type;
 bool poisoned,atomic_wb,atomic_uc;
};
struct trl_freeze {
 struct td_ticket directory;
 struct trl_backing backing;
 uint64_t lease_version;
 unsigned type;
};
struct trl_snapshot {
 struct td_snapshot root;
 struct trl_token token;
 uint64_t receipt_type_epoch,receipt_version;
 /* false for receipts AND retired records, even when HPA is reused. */
 bool scan_allowed;
};
/* These are explicit caller attestations, not hardware receipts computed here.
 * They bind completed full-domain maintenance to the exact sealed inventory.
 * No bool/mask below may be manufactured from token/refcount state. */
struct trl_transition_evidence {
 struct trl_freeze freeze;
 uint64_t new_epoch,serial,closed,drained,tlb_before,cache_before,tlb_after,cache_after;
 unsigned destination;
 bool complete_alias_audit,all_refills_revoked,all_roots_invalidated;
 bool host_break_before_make,all_aliases_retyped,owner_stamps_ready;
};
struct trl_final_evidence {
 struct trl_freeze freeze;
 uint64_t serial,parked,vmcs_gone,refills_revoked,roots_flushed,caches_flushed;
 bool restored_wb,host_wb_verified,complete_alias_audit;
};
/* Fresh objects only; no reinitialization of a live lease or old DTL. Initial
 * target type is WB. Budget counts stable slots, including durable receipts.
 * Retired slots are not recycled until directory teardown. */
bool trl_init(struct typed_root_lease *,struct typed_directory *,struct trl_backing,size_t budget);
/* Separate immutable WB/IPAT1/AD1 contract. No UC/WT/PAT0 or type transition.
 * It does not audit aliases, pin source backing or authorize guest entry. */
bool trl_init_atomic_wb(struct typed_root_lease *,struct typed_directory *,struct trl_backing,size_t budget);
/* Fixed UC/IPAT1/AD1 dedicated data policy. Before admitting any token or
 * source consumer, caller proves current physical UC, WB provenance, every
 * alias and lifetime. An unpublished bootstrap may bind this policy first,
 * but must keep admission closed until that proof. No live retype/release. */
bool trl_init_atomic_uc(struct typed_root_lease *,struct typed_directory *,struct trl_backing,size_t budget);
bool trl_reserve(struct typed_root_lease *,struct td_operation *,uint64_t,
 struct trl_binding *,uint64_t owner,uint64_t allocator_lifetime);
bool trl_bind_candidate(struct typed_root_lease *,struct td_operation *,uint64_t,
 struct trl_binding *,struct td_record *,struct td_identity);
/* Run after actual td_attach. Generation zero remains owned but not usable. */
bool trl_attach(struct typed_root_lease *,struct td_operation *,uint64_t,struct trl_binding *);
/* Validate typed candidate BEFORE td_publish, then reconcile after success. */
bool trl_publish_ready(struct typed_root_lease *,struct td_operation *,uint64_t,struct trl_binding *);
bool trl_publish(struct typed_root_lease *,struct td_operation *,uint64_t,struct trl_binding *);
bool trl_retain(struct typed_root_lease *,struct td_operation *,uint64_t,struct trl_binding *);
/* Only after td_cancel (or failed td_reserve), never after root allocation. */
bool trl_cancel(struct typed_root_lease *,struct td_operation *,uint64_t,struct trl_binding *);
bool trl_token_read(const struct typed_root_lease *,const struct trl_binding *,struct trl_token *);
bool trl_admit(struct typed_root_lease *,struct td_operation *,uint64_t,
 struct trl_binding *,struct trl_token,unsigned type,bool ipat,bool ad);
/* Root-cache retirement is legal in UC and never releases data backing.
 * Call after actual td_retire_commit, before any tree/metadata free. The
 * still-stable record may already be unlinked by td_forget. Commit is one-shot.
 * Preserve record + binding + receipt on failure. Proof retains ticket epoch
 * and caller-captured exact commit version independently of later snapshot
 * epoch/version. Use the durable receipt.commit_version from td_retire_commit, preserve it
 * on retry, and never reconstruct it from a later d->version. The directory
 * receipt also binds the exact retirement nonce and type epoch. */
bool trl_retire(struct typed_root_lease *,struct td_operation *,uint64_t,
 struct trl_binding *,struct trl_token,struct td_retire_ticket,const struct td_receipt *,uint64_t receipt_version);
/* Conservative first model: caller drains admitted operations before close.
 * Refuse pending/gen0/unbound roots and registered refill sources without
 * closing, so their ordinary retirement/cleanup remains possible. The real
 * td gate then closes and td_seal independently rechecks drain. */
bool trl_close(struct typed_root_lease *);
bool trl_seal(struct typed_root_lease *,struct trl_freeze *);
bool trl_freeze_valid(const struct typed_root_lease *,struct trl_freeze);
bool trl_snapshot_read(const struct typed_root_lease *,struct trl_freeze,
 struct trl_snapshot *,size_t capacity,size_t *count);
/* Index is temporary for this exact snapshot, never token identity. */
bool trl_snapshot_index(const struct typed_root_lease *,struct trl_freeze,
 struct td_identity,bool receipt,size_t *index);
/* Prepare all outstanding root tokens, including retained allocated roots.
 * Generation-zero candidates cannot transition; retire them first. Old value
 * tokens remain unchanged until reopen, and are rejected after epoch advance. */
bool trl_prepare_token(struct typed_root_lease *,struct trl_freeze,
 struct trl_binding *,struct trl_token,uint64_t new_epoch,unsigned destination,struct trl_token *);
bool trl_reopen(struct typed_root_lease *,struct trl_freeze,
 const struct trl_transition_evidence *);
/* Legacy AD0 query only, never frees. Atomic profile rejects: this evidence
 * does not cover catalog/updater lifetime reclamation. Requires sealed empty
 * live-root inventory, WB,
 * zero tokens and complete independent final external maintenance evidence. */
bool trl_backing_release_ready(const struct typed_root_lease *,struct trl_freeze,
 const struct trl_final_evidence *);
void trl_poison(struct typed_root_lease *);
#endif
