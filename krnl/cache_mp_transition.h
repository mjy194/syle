#ifndef SYLE_CACHE_MP_TRANSITION_H
#define SYLE_CACHE_MP_TRANSITION_H
#include <stdbool.h>
#include <stdint.h>
enum cache_mp_phase {
    CACHE_MP_OPEN_WB, CACHE_MP_DETACH_EPT, CACHE_MP_BREAK_TLB,
    CACHE_MP_BREAK_CACHE, CACHE_MP_MAKE_TLB, CACHE_MP_MAKE_CACHE,
    CACHE_MP_PUBLISH_EPT, CACHE_MP_COMPLETE, CACHE_MP_OPEN_UC, CACHE_MP_POISON
};
struct cache_mp_transition {
    uint64_t participants,generation,acknowledged;
    enum cache_mp_phase phase,destination;
    bool published;
};
/* Pure BSP-serialized bookkeeping, never an AP-mutated shared object.
 * Fresh init only; participant set stays fixed. There is no recovery from
 * POISON, no allocation, no privileged instruction, no ownership inference. */
bool cache_mp_transition_init(struct cache_mp_transition *,uint64_t participants);
bool cache_mp_transition_begin(struct cache_mp_transition *,uint64_t expected_epoch,
    enum cache_mp_phase destination,bool all_quiesced,bool resources_ready,uint64_t *epoch);
/* Successful action attestations, NOT commands: caller first actually removes
 * guest EPT mapping, clears host PTE, installs new host PTE, or publishes EPT,
 * respectively. Required once before that phase's per-CPU acknowledgements.
 * BREAK_CACHE/MAKE_CACHE need only local successful cache-operation ACKs. */
bool cache_mp_transition_publish(struct cache_mp_transition *,uint64_t epoch,enum cache_mp_phase);
bool cache_mp_transition_ack(struct cache_mp_transition *,unsigned cpu,uint64_t epoch,enum cache_mp_phase);
/* Full participant coverage then explicit advance, never automatic opening. */
bool cache_mp_transition_advance(struct cache_mp_transition *,uint64_t epoch);
bool cache_mp_transition_finish(struct cache_mp_transition *,uint64_t epoch);
bool cache_mp_transition_entry_allowed(const struct cache_mp_transition *,uint64_t epoch);
/* Timeout/partial/uncertain effects demand permanent poison and retained
 * resources. Failed calls otherwise do not mutate state or output values. */
void cache_mp_transition_fail(struct cache_mp_transition *);
#endif
