#ifndef SYLE_LINUX_GUEST_IRQ_GLUE_H
#define SYLE_LINUX_GUEST_IRQ_GLUE_H
#include "linux_irq_glue.h"
#include "linux_guest_irq_adapter.h"
/* New guest_apic whole-domain transaction; never aliases legacy lia. Same
 * receipt/quarantine lifecycle as lig, with source-aware reservation and
 * guest_apic_accept preserving TMR/pending_level. Caller must not mutate any
 * model (including timer poll) between staging and after-entry receipt. */
bool lgig_before(struct lig_state*,struct gia*,const struct lei_snapshot*,void*,lei_write);
bool lgig_after(struct lig_state*,struct gia*,enum lig_outcome,uint32_t,uint32_t,void*,lei_write);
#endif
