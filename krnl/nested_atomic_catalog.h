#ifndef SYLE_NESTED_ATOMIC_CATALOG_H
#define SYLE_NESTED_ATOMIC_CATALOG_H
#include "nested_root_catalog.h"
#include "nested_ept_atomic.h"
#include "ad_updater_gate.h"
/* Cache allocation lifetime and cache generation are separate from structural
 * source generation and AD epoch. Owner may be zero. Full physical EPTP is
 * AD1; a reused root HPA alone never identifies the same allocation. */
struct nac_cache_identity { uint64_t owner,hpa,full_eptp,lifetime,generation; };
/* Trusted local-owner guard validates the ACTUAL current owned cache root,
 * allocation lifetime, generation, stopped consumer and external refill permit.
 * Trusted context must also prove that the actual cache virtual key equals
 * permit.source.root.full_eptp under that same admitted external operation.
 * NAC does not register this cache/source pairing in any directory itself.
 * Called unlocked; must not reenter or mutate the operation. */
typedef bool (*nac_cache_guard_fn)(void *,struct nac_cache_identity);
struct nac_ticket { uint64_t source_nonce,updater_nonce; };
/* Immutable source policy. Epoch zero denotes the legacy all-WB policy.
 * UC type epoch never advances with A/D maintenance or cache generations. */
struct nac_type_policy {uint64_t epoch,uc_gpa,uc_hpa;};
struct nac_permit {
 struct nac_type_policy type_policy;
 struct nrc_session source;
 struct ad_updater_token updater;
 struct ad_updater_gate *gate;
 struct ad_entry_stamp stamp;
 struct nac_cache_identity cache;
 struct nac_ticket ticket;
 nac_cache_guard_fn cache_guard;
 void *cache_context;
 bool active;
};
struct nac_trace {
 struct nac_type_policy type_policy;
 struct nrc_root source;
 struct nac_cache_identity cache;
 uint64_t gpa;
 struct nested_ept_atomic_trace path; /* includes the independent AD stamp */
 struct nested_ept_result translation;
 bool complete;
};
/* Pure substrate, not a VMX/registry bridge. Caller retains BOTH source pins
 * and updater admission through walk/CAS, tracked fill or promotion, actual
 * local INVEPT and final guard. walk never releases them. A successful begin
 * alone is not guest-entry authority, physical pinning or retirement evidence.
 * Gate stamp domain/incarnation/context/lifetime binds the entire catalog;
 * its epoch is an AD epoch, never the source structural generation.
 * Stable zero-initialized owner-exclusive permit; never move/reinitialize live
 * storage. Save returned ticket separately; do not reread permit.ticket for
 * later operations. Failed begin can transfer retained resources: output ticket
 * is set once a source session exists, even when false is returned. active
 * remains true until cleanup really releases everything. */
bool nac_begin(struct nested_root_catalog *,struct nrc_root,void *authority_context,
 struct ad_updater_gate *,struct ad_entry_stamp,struct nac_cache_identity,
 nac_cache_guard_fn,void *cache_context,struct nac_permit *,struct nac_ticket *);
bool nac_guard(struct nac_permit *,struct nac_ticket);
/* Pure cleanup, not proof that publication succeeded. Releases updater first,
 * then source. Failed release poisons admissions and retains stable resources.
 * A poisoned permit may be cleaned up using its original saved ticket. */
bool nac_end(struct nac_permit *,struct nac_ticket);
/* Final full guard plus cleanup; false is fail-closed even if cleanup succeeded.
 * Caller must retain any possibly installed tree on failure, not roll it back. */
bool nac_complete(struct nac_permit *,struct nac_ticket);
struct nested_ept_atomic_result nac_walk(struct nac_permit *,struct nac_ticket,
 uint64_t gpa,uint8_t need,struct nac_trace *);
/* BEFORE any cached write CAS: bind every durable identity to a fresh permit,
 * then re-resolve every table and atomically validate the path and final backing.
 * Old operation nonces do not authorize a cached trace. No shared-word writes.
 * Trace storage alone provides NO persistent retention/refill authority. Before
 * caching beyond a permit, caller must register/pin all source and cache backing
 * and revoke these traces BEFORE retirement receipt/free. Not wired by N41A. */
bool nac_trace_guard(struct nac_permit *,struct nac_ticket,uint64_t expected_gpa,const struct nac_trace *);
#endif
