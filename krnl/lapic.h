#ifndef SYLE_LAPIC_H
#define SYLE_LAPIC_H
#include <stdbool.h>
#include <stdint.h>

/* Single-vCPU diagnostic x2APIC subset, not an OS-compatible APIC.
 * No host MSR access, MMIO, timer, LVT, ICR, ESR, routing or APICv. */
struct lapic {
    uint64_t base;
    uint32_t irr[8], isr[8];
    uint8_t tpr;
    uint16_t svr;
};
void lapic_init(struct lapic *a);
/* Unsupported/invalid accesses return false without changing state/output. */
bool lapic_read(const struct lapic *a, uint32_t msr, uint64_t *value);
bool lapic_write(struct lapic *a, uint32_t msr, uint64_t value);
uint8_t lapic_ppr(const struct lapic *a);
/* Selection does not consume pending state. Commit only after successful
 * guest entry, before emulating a subsequent guest register access.
 * On failed entry do NOT call accept: IRR remains pending.
 * Caller must not mutate this single-vCPU model between select and accept. */
bool lapic_select(const struct lapic *a, uint8_t *vector);
bool lapic_accept(struct lapic *a, uint8_t vector);
/* Narrow diagnostic delivery acknowledgement, not generic vectoring recovery.
 * All other exits while injection is unconfirmed must stop without reentry. */
bool lapic_irq_ack_valid(uint64_t reason, uint64_t rip, uint64_t first_handler_rip,
                         uint64_t vectoring);
bool lapic_window_caps(uint64_t primary_caps);
bool lapic_window_arm(const struct lapic *a, bool armed, bool injected,
    uint64_t pending, uint64_t activity);
bool lapic_window_ready(const struct lapic *a, uint64_t reason, uint64_t flags,
    uint64_t blocking, uint64_t activity, uint64_t pending, uint64_t vectoring,
    uint8_t *vector);
bool lapic_collision_ud_ready(const struct lapic *a, uint64_t reason,
    uint64_t info,uint64_t blocking,uint64_t flags,uint64_t activity,
    uint64_t pending,uint64_t vectoring,bool already_injected);
#endif
