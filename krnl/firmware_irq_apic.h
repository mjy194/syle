#ifndef SYLE_FIRMWARE_IRQ_APIC_H
#define SYLE_FIRMWARE_IRQ_APIC_H
#include "firmware_apic_transaction.h"
#include "firmware_timer_irq_session.h"
/* Separate timer-only profile. Quiet config remains event-free; session.owner
 * is sole IRR/ISR source. Project only into temporary guest_apic for reads,
 * priority and EOI. No guest_apic_poll or legacy timer sampling. */
bool fiap_select(const struct fag_state*,const struct ftis_session*,bool*ready);
enum fat_result fiap_mmio(struct fag_state*,struct ftis_session*,uint64_t[15],
 const struct fmm_memory*,const struct fat_exit*,void*,lei_write);
#endif
