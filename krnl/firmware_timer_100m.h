#ifndef SYLE_FIRMWARE_TIMER_100M_H
#define SYLE_FIRMWARE_TIMER_100M_H
#include <stdbool.h>
#include <stdint.h>
/* NONDEFAULT pure prototype. Explicit virtual APIC bus 100MHz, reference
 * 10MHz Hv#1 counter. This is not a physical-APIC or calibrated-TSC claim.
 * Owner must independently admit reference and epoch continuity. Fresh
 * zeroed private state; terminal latch must never be snapshot-rolled-back. */
struct ft100 {
 uint64_t epoch,origin,last,deadline_bus,period_bus,max_gap;
 uint32_t initial,lvt;unsigned divisor;
 bool ready,armed,terminal;
};
enum ft100_result {FT100_BAD,FT100_OK,FT100_STOP};
bool ft100_init(struct ft100*,uint64_t epoch,uint64_t reference,uint64_t max_gap);
/* Observes old events first: BAD new configuration may still advance last or
 * consume masked expiry. Not an atomic MMIO API. Never roll back terminal. */
enum ft100_result ft100_start(struct ft100*,uint64_t epoch,uint64_t reference,uint32_t count,unsigned divisor,uint32_t lvt);
enum ft100_result ft100_sample(struct ft100*,uint64_t epoch,uint64_t reference,uint32_t *current);
/* Entry safety predicate only, NOT a VMX programmer/certificate generator.
 * latest_reference is a separately proven upper bound on the next exit sample.
 * Unknown bound must NOT call this API or enter. Permit only strictly before
 * expiry, including fractional 100ns boundaries. Invalid bounds fail closed. */
bool ft100_entry_before(const struct ft100*,uint64_t latest_reference);
#endif
