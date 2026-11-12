#ifndef SYLE_CACHE_RENDEZVOUS_H
#define SYLE_CACHE_RENDEZVOUS_H
#include <stdbool.h>
#include <stdint.h>
enum cache_rendezvous_phase {
    CACHE_RENDEZVOUS_OPEN, CACHE_RENDEZVOUS_PRECHECK, CACHE_RENDEZVOUS_TLB,
    CACHE_RENDEZVOUS_CACHE, CACHE_RENDEZVOUS_COMPLETE, CACHE_RENDEZVOUS_POISON
};
struct cache_rendezvous {
    uint64_t participants,generation,acknowledged;
    enum cache_rendezvous_phase phase;
};
/* Pure BSP-serialized protocol state. Participants are fixed for its lifetime.
 * AP replies are consumed by BSP, never concurrent mutations of this object.
 * Fresh init only, not poison recovery. No allocation or hardware operations. */
bool cache_rendezvous_init(struct cache_rendezvous *,uint64_t participants);
/* Caller proves all participating CPUs stopped and reserves all resources
 * BEFORE begin. On failure no epoch change and output remains unchanged. */
bool cache_rendezvous_begin(struct cache_rendezvous *,uint64_t expected_epoch,
    bool all_quiesced,bool resources_ready,uint64_t *new_epoch);
/* Ack only after the named CPU actually completed that local phase. The pure
 * model checks provenance identifiers, not whether hardware work occurred. */
bool cache_rendezvous_ack(struct cache_rendezvous *,unsigned cpu,uint64_t epoch,
    enum cache_rendezvous_phase phase);
/* Full coverage required; final ACK never advances or opens entry itself. */
bool cache_rendezvous_advance(struct cache_rendezvous *,uint64_t epoch);
/* COMPLETE still blocks entry; caller must explicitly finish. */
bool cache_rendezvous_finish(struct cache_rendezvous *,uint64_t epoch);
bool cache_rendezvous_entry_allowed(const struct cache_rendezvous *,uint64_t epoch);
/* Timeout/uncertain failure: permanently closes entry, retaining bookkeeping.
 * No recovery/rollback/automatic resource reclamation is implied. */
void cache_rendezvous_fail(struct cache_rendezvous *);
#endif
