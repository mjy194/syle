#ifndef SYLE_FIRMWARE_TIMER_IRQ_SESSION_H
#define SYLE_FIRMWARE_TIMER_IRQ_SESSION_H
#include "firmware_timer_irq_owner.h"
#include "linux_external_irq.h"
/* HOST ONLY. Native caller must bind these identities to the actual selected
 * VMCS and its exact assembly entry attempt. Outcome parameters are evidence
 * inputs, NOT authenticated/proven by this model. No receipt whitelist growth.
 * Fresh init is NOT an import of an already-running ft100 clock. A future
 * adapter must preserve its origin/deadline/phase, never restart at first due.
 * Fresh private serialized storage; no mutation/copy/rollback/reinitialization.
 * Only this session may invoke its owner cancellation/accounting APIs. */
struct ftis_session {
 struct fti_owner owner;
 struct lei_irq event;
 uint64_t domain,vmcs,attempts,active_attempt;
 bool ready,failed,busy;
};
enum ftis_result {FTIS_FATAL=-1,FTIS_IDLE,FTIS_DEFER,FTIS_STAGED,
 FTIS_ACCOUNTED,FTIS_STOP_NO_ENTRY};
bool ftis_init(struct ftis_session*,uint64_t domain,uint64_t vmcs,
 uint64_t reference,uint64_t max_gap);
bool ftis_sample(struct ftis_session*,uint64_t domain,uint64_t vmcs,uint64_t reference);
enum ftis_result ftis_before(struct ftis_session*,uint64_t domain,uint64_t vmcs,
 const struct lei_snapshot*,void*,lei_write,uint64_t *attempt);
enum ftis_result ftis_after(struct ftis_session*,uint64_t domain,uint64_t vmcs,
 uint64_t attempt,bool vmfail,bool real_exit,uint32_t reason,uint32_t vectoring,
 void*,lei_write);
/* Host-only retirement after independently committed EOI, not a MMIO txn. */
bool ftis_eoi(struct ftis_session*,uint64_t domain,uint64_t vmcs);
#endif
