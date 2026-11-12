#ifndef SYLE_LAPIC_TIMER_H
#define SYLE_LAPIC_TIMER_H
#include <stdbool.h>
#include <stdint.h>

/* Experimental virtual one-shot/optional periodic timer: one undivided tick = 1024 host TSC
 * ticks. This is NOT a claim about physical APIC/bus frequency or VMX ticks.
 * Caller polls before register accesses/entry and gates IRQ delivery through
 * the virtual LAPIC. All operations reject backwards time without mutation. */
#define LAPIC_TIMER_BASE_TICKS UINT64_C(1024)
struct lapic_timer {
    uint32_t lvt, divide, initial;
    uint64_t deadline, last_now, period, last_expirations;
    bool armed, periodic_supported;
};
enum lapic_timer_event {
    LAPIC_TIMER_INVALID,
    LAPIC_TIMER_NONE,
    LAPIC_TIMER_MASKED,
    LAPIC_TIMER_IRQ
};
/* Legacy initializer never advertises/supports periodic mode. */
void lapic_timer_init(struct lapic_timer *t, uint64_t now);
void lapic_timer_init_mode(struct lapic_timer *t, uint64_t now,
                           bool periodic_supported);
/* Supported MSRs: 832 LVT, 838 initial, 839 current (RO), 83e divider.
 * Successful reads/writes advance last_now. Rejected operations leave t and
 * output unchanged. Configuration writes require !armed, including same-
 * value writes; initial=0 cancels and initial>0 restarts an active timer.
 * Periodic bit17 is allowed only with periodic_supported. Bit18 is rejected.
 * Current-count reads compute the current periodic phase even before poll,
 * but do not consume an expiry or move its stored deadline. Overflow rejects
 * the read atomically. Poll before entry/IRQ accounting remains required. */
bool lapic_timer_read(struct lapic_timer *t, uint32_t msr,
                      uint64_t now, uint64_t *value);
bool lapic_timer_write(struct lapic_timer *t, uint32_t msr,
                       uint64_t value, uint64_t now);
/* One-shot expiry disarms. Periodic expiry advances deadline from its OLD
 * phase by k complete periods, remains armed, and returns only ONE IRQ or
 * MASKED result even if k>1. last_expirations=k on IRQ/MASKED, zero on NONE
 * or initial-count rewrite; INVALID leaves all state/output unchanged.
 * No catch-up queue: unmask does not replay a consumed masked expiry.
 * vector is written only on IRQ. NONE/MASKED/INVALID leave it unchanged. */
enum lapic_timer_event lapic_timer_poll(struct lapic_timer *t,
                                       uint64_t now, uint8_t *vector);
/* Nonmutating conversion of remaining absolute-deadline TSC ticks to VMX
 * ticks. ceil(delta / 2^shift), clipped to UINT32_MAX, shift in [0,31].
 * Inactive or expired timer returns zero. Call poll to consume expiry. */
bool lapic_timer_reload(const struct lapic_timer *t, uint64_t now,
                         unsigned shift, uint32_t *ticks);
/* pin bit6 and exit bit22 must BOTH be settable and clearable. */
bool lapic_timer_caps(uint64_t pin_caps, uint64_t exit_caps);
#endif
