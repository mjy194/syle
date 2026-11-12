#ifndef LINUX_EXTERNAL_IRQ_H
#define LINUX_EXTERNAL_IRQ_H
#include <stdbool.h>
#include <stdint.h>
enum lei_phase { LEI_IDLE, LEI_STAGED, LEI_DELIVERED, LEI_QUARANTINED };
enum lei_gate { LEI_REJECT, LEI_DEFER_WINDOW, LEI_READY };
struct lei_snapshot {
 uint64_t rflags;
 uint32_t blocking, activity, entry_info, vectoring;
 bool pf_pending, high_pending;
};
struct lei_irq { enum lei_phase phase; uint64_t token; uint8_t vector; bool armed, cancellable, cleaned; };
typedef bool (*lei_write)(void *, uint32_t, uint64_t);
typedef bool (*lei_accept)(void *, uint64_t, uint8_t);
/* Fresh/quiescent context only. Caller owns single-BSP serialization. */
void lei_init(struct lei_irq *);
enum lei_gate lei_check(const struct lei_irq *, unsigned, const struct lei_snapshot *);
/* False can leave VMCS partially changed: NO ENTRY; token stays owned. */
bool lei_stage(struct lei_irq *, unsigned, uint64_t, const struct lei_snapshot *, void *, lei_write);
/* Caller supplies verified native-entry outcome, not entry.valid as receipt.
 * vmfail means no entry; entry_failed is exit-reason bit31. Contradictions quarantine. */
void lei_observe(struct lei_irq *, bool vmfail, bool real_exit, uint32_t reason, uint32_t vectoring);
/* Only after definite no-entry or failed stage. No LAPIC consume. */
bool lei_cancel(struct lei_irq *, void *, lei_write);
/* Clear consumed metadata before accounting; failure retains receipt, NO ENTRY. */
bool lei_clear_delivered(struct lei_irq *, void *, lei_write);
/* DELIVERED and cleaned only; failure retains receipt, retry ONLY this bookkeeping. */
bool lei_account(struct lei_irq *, void *, lei_accept);
#endif
