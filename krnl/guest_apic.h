#ifndef SYLE_GUEST_APIC_H
#define SYLE_GUEST_APIC_H
#include "lapic.h"
#include "lapic_timer.h"
/* Opt-in single-BSP xAPIC/x2APIC register device profile. Not a complete Linux platform.
 * Guest exposure is caller-owned: do not advertise until CPUID, VM-exit/MSR
 * routing, firmware topology and event transactions are coherent.
 * No host MSR/APIC access. All functions are single-thread/serialized. */
struct guest_apic {
    struct lapic irq; /* priority core, internal x2APIC register encoding */
    bool x2apic;
    uint32_t ldr, dfr;
    struct lapic_timer timer;
    uint32_t lvt[6]; /* CMCI, thermal, perf, LINT0, LINT1, error */
    uint32_t tmr[8];
    uint32_t pending_level[8];
    uint64_t icr;
};
struct guest_apic_effect { bool eoi, level; uint8_t vector; };
void guest_apic_init(struct guest_apic *, uint64_t now); /* explicit x2APIC */
void guest_apic_init_mode(struct guest_apic *,uint64_t now,bool x2apic);
/* xAPIC MMIO register offsets only; caller owns mapping and instruction decode.
 * Aligned 32-bit register access. Reject in x2APIC mode. */
bool guest_apic_mmio(struct guest_apic *,uint64_t offset,unsigned width,
                    bool write,uint32_t *value,uint64_t now,
                    struct guest_apic_effect *);
/* Invalid access atomically leaves model/output/effect unchanged. Successful
 * write resets effect, setting eoi only when an ISR vector was retired.
 * Route effect to IOAPIC after commit; never replay register write on callback
 * failure. Values are x2APIC MSRs; no MMIO/mode aliases are implied. */
bool guest_apic_read(struct guest_apic *, uint32_t msr,uint64_t now,uint64_t *);
bool guest_apic_write(struct guest_apic *,uint32_t msr,uint64_t value,
                       uint64_t now,struct guest_apic_effect *);
/* Fixed delivery; low vectors/disabled software APIC rejected without change.
 * Duplicate pending/in-service vector must use same trigger mode. IRR stores one event,
 * not a count. level pending and in-service trigger state are kept separately. */
bool guest_apic_enqueue(struct guest_apic *,unsigned vector,bool level);
bool guest_apic_select(const struct guest_apic *,uint8_t *);
bool guest_apic_accept(struct guest_apic *,uint8_t vector);
/* Poll timer into IRR; expiry consumed while software APIC disabled, no IRQ.
 * Invalid time/overflow leaves model unchanged. Coalesces multiple expiries. */
bool guest_apic_poll(struct guest_apic *,uint64_t now);
#endif
