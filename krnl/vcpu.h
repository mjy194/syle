#ifndef SYLE_VCPU_H
#define SYLE_VCPU_H
#include <stdint.h>
#include <stdbool.h>
enum vcpu_state { VCPU_RUNNABLE, VCPU_HALTED, VCPU_FAULTED, VCPU_COMPLETED };
struct vcpu { enum vcpu_state state; bool ud_injected, ud_handled, irq_pending, irq_handled, gp_pending, gp_handled, pf_pending, pf_handled; };
/* IA32_VMX_MISC[6]: VM-entry supports guest activity HLT. */
static inline bool vcpu_halt_activity_supported(uint64_t misc) {
    return (misc & (UINT64_C(1)<<6)) != 0;
}
/* This policy handles exactly one synchronous #UD in an active trusted guest.
 * No async queue, interrupt window or NMI is implemented; synthetic wakeup below. */
bool vcpu_inject_ud(struct vcpu *v, uint64_t reason, uint64_t info,
    uint64_t vectoring, uint64_t pending, uint64_t blocking, uint64_t activity,
    uint64_t rip, uint64_t expected, uint32_t *entry_info);
bool vcpu_ack_ud(struct vcpu *v);
bool vcpu_halt(struct vcpu *v);
bool vcpu_complete(struct vcpu *v);
/* Synthetic host event only; blocked injection fails closed, no queue/window. */
bool vcpu_inject_irq(struct vcpu *v, uint32_t vector, uint64_t flags,
    uint64_t blocking, uint64_t activity, uint64_t pending, uint64_t vectoring,
    uint32_t *entry_info);
bool vcpu_ack_irq(struct vcpu *v, uint64_t activity);
bool vcpu_inject_gp(struct vcpu *v, uint64_t reason, uint64_t info, uint64_t error,
    uint64_t vectoring, uint64_t pending, uint64_t blocking, uint64_t activity,
    uint64_t rip, uint64_t expected, uint32_t *entry_info, uint32_t *entry_error);
bool vcpu_ack_gp(struct vcpu *v);
bool vcpu_inject_pf(struct vcpu *v, uint64_t reason, uint64_t info, uint64_t error,
    uint64_t gla, uint64_t vectoring, uint64_t pending, uint64_t blocking, uint64_t activity,
    uint64_t rip, uint64_t expected, uint32_t *entry_info, uint32_t *entry_error);
bool vcpu_ack_pf(struct vcpu *v);
void vcpu_fault(struct vcpu *v);
#endif
