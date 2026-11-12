#ifndef SYLE_LINUX_IRQ_GLUE_H
#define SYLE_LINUX_IRQ_GLUE_H
#include "linux_external_irq.h"
#include "linux_irq_adapter.h"
enum lig_outcome { LIG_NO_ENTRY,LIG_EXIT,LIG_UNKNOWN };
struct lig_state {struct lei_irq event;bool failed,busy;};
/* Called only with exclusive BSP ownership, before any guest state is loaded.
 * A false return forbids entry and retains all reservation ownership. */
bool lig_before(struct lig_state*,struct lia*,const struct lei_snapshot*,void*,lei_write);
/* Called once after host-safe restoration, before any device mutation or exit
 * emulation. UNKNOWN never means no-entry. False permanently forbids resume.
 * Never reset a nonidle/failed object unless the entire guest+device domain has
 * been destroyed; cancellation needs current VMCS, not a post-VMXOFF cleanup. */
bool lig_after(struct lig_state*,struct lia*,enum lig_outcome,uint32_t,uint32_t,void*,lei_write);
#endif
