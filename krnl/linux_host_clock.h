#ifndef SYLE_LINUX_HOST_CLOCK_H
#define SYLE_LINUX_HOST_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
#define LHC_HZ UINT64_C(1193182)
#define LHC_LIMIT (UINT64_MAX / UINT64_C(1000))
/* Source is externally verified, invariant and pinned; no guessed frequency.
 * epoch changes on host suspend, TSC adjustment, migration or recalibration.
 * All methods are pure state transactions: false leaves state unchanged;
 * the integrating owner MUST fail-stop, never retry with a fabricated sample. */
struct lhc_source {uint64_t hz,epoch;uint32_t cpu;bool verified,invariant;};
struct lhc_sample {uint64_t tsc,epoch;uint32_t cpu;};
struct lhc_clock {
 struct lhc_source source;
 uint64_t last_tsc,tick,fraction,max_gap;
 bool ready,running;
};
bool lhc_init(struct lhc_clock*,const struct lhc_source*,struct lhc_sample,uint64_t max_gap);
bool lhc_start(struct lhc_clock*,struct lhc_sample);
bool lhc_advance(struct lhc_clock*,struct lhc_sample);
bool lhc_stop(struct lhc_clock*,struct lhc_sample);
#endif
