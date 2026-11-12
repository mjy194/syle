#ifndef SYLE_SHOOTDOWN_H
#define SYLE_SHOOTDOWN_H
#include <stdbool.h>
#include <stdint.h>
enum shootdown_phase { SHOOTDOWN_READY, SHOOTDOWN_PUBLISHING,
                       SHOOTDOWN_WAIT, SHOOTDOWN_POISON };
struct shootdown {
    enum shootdown_phase phase;
    uint64_t participants, running, acknowledged, generation;
};
/* Pure sequencing model, exclusively owned by a serialized BSP controller.
 * APs send mailbox replies; they never mutate this object concurrently.
 * Init is for a fresh object ONLY, not recovery/reuse of a poisoned domain.
 * Participants identify CPUs that may retain translations, not just running
 * CPUs; all participants must acknowledge each publication epoch.
 * No hardware invalidation, barriers, IPI delivery or reclamation is implied. */
bool shootdown_init(struct shootdown *, uint64_t participants);
bool shootdown_enter(struct shootdown *, unsigned cpu, uint64_t generation);
bool shootdown_leave(struct shootdown *, unsigned cpu);
/* Caller must mark enter BEFORE permitting VM entry and leave only AFTER a
 * verified exit. Begin reserves a new epoch, closes entry, requires all CPUs
 * stopped. Caller publishes mapping, then calls published, then gathers real
 * local invalidation completions. Any uncertain outcome must call fail. */
bool shootdown_begin(struct shootdown *, uint64_t expected_generation,
                     uint64_t *new_generation);
bool shootdown_published(struct shootdown *);
bool shootdown_ack(struct shootdown *, unsigned cpu, uint64_t generation);
/* Poison retains accounting; only external teardown after verified CPU stop
 * can release resources. No API resumes a poisoned domain. */
void shootdown_fail(struct shootdown *);
#endif
