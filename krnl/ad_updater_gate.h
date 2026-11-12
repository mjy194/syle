#ifndef SYLE_AD_UPDATER_GATE_H
#define SYLE_AD_UPDATER_GATE_H
#include <stdbool.h>
#include <stdint.h>
#define AD_UPDATER_LOCK_TRIES 1048576u
#include "ad_entry_merge.h"
struct ad_updater_gate;
struct ad_updater_token {
    struct ad_updater_gate *gate;
    struct ad_updater_token *next;
    struct ad_entry_stamp stamp;
    uint64_t nonce;
    bool live;
};
struct ad_updater_gate {
 /* Immutable construction provenance; NULL for ordinary constructors. */
 const struct ad_updater_gate *predecessor;
    uint32_t lock,fault;
    struct ad_entry_stamp stamp;
    struct ad_updater_token *tokens;
    uint64_t references,next_nonce;
    bool closed,initialized,superseded;
};
/* Fresh gate only; caller never reuses a domain/incarnation/context/lifetime
 * identity for another lifetime. Caller supplies zero-initialized, stable,
 * exclusive token storage, not shared across concurrent acquire attempts.
 * All metadata access goes through these APIs. Caller pins table ownership
 * through the WHOLE lease and closed drain; APIs do not validate memory maps.
 * Acquire returns an immutable nonce ticket; release/guard must use that saved
 * value, not read token.nonce anew. This prevents same-storage/same-epoch ABA.
 * No metadata lock is held during the caller's walk/CAS. */
bool ad_updater_gate_init(struct ad_updater_gate *,struct ad_entry_stamp);
/* Fresh zero-initialized exclusive successor, distinct storage and fresh
 * context/lifetime in the same domain/incarnation. Consumes a closed drained
 * predecessor once, inheriting its locked final nonce highwater. Nonzero
 * reserve_nonces must fit. Old becomes terminal and cannot reopen. */
bool ad_updater_gate_init_successor(struct ad_updater_gate *new_gate,
 struct ad_updater_gate *old_gate,struct ad_entry_stamp old_stamp,
 struct ad_entry_stamp new_stamp,uint64_t reserve_nonces);
bool ad_updater_gate_acquire(struct ad_updater_gate *,struct ad_entry_stamp,struct ad_updater_token *,uint64_t *nonce_out);
bool ad_updater_gate_release(struct ad_updater_gate *,struct ad_updater_token *,struct ad_entry_stamp,uint64_t nonce);
bool ad_updater_gate_guard(struct ad_updater_gate *,struct ad_updater_token *,struct ad_entry_stamp,uint64_t nonce);
bool ad_updater_gate_close(struct ad_updater_gate *,struct ad_entry_stamp);
/* Snapshot only: safe as maintenance evidence solely when every consumer and
 * other updater source is independently stopped. Does not close admission. */
/* Admission snapshot, not a drain or consumer synchronization predicate. */
bool ad_updater_gate_open(struct ad_updater_gate *,struct ad_entry_stamp);
bool ad_updater_gate_idle(struct ad_updater_gate *,struct ad_entry_stamp);
bool ad_updater_gate_drained(struct ad_updater_gate *,struct ad_entry_stamp);
/* Non-consuming locked current-bank headroom snapshot, not reserved admission.
 * Requires exact closed/drained/nonterminal bank; output unchanged on false.
 * Existing bounded-lock exhaustion may latch fault. */
bool ad_updater_gate_successor_budget(struct ad_updater_gate *,struct ad_entry_stamp,uint64_t reserve,uint64_t *nonce);
/* Attestation is caller's completed all-consumer invalidation, not hardware
 * proof. Reopen increments epoch; old stamps can never acquire/release anew. */
bool ad_updater_gate_reopen(struct ad_updater_gate *,struct ad_entry_stamp,bool invalidated,struct ad_entry_stamp *out);
void ad_updater_gate_poison(struct ad_updater_gate *);
/* Bounded lock exhaustion poisons permanently; no owner lock stealing.
 * Valid existing leases may still release after poison, but cannot reopen.
 * A false drain result (including lock failure) never authorizes reclamation. */
#endif
