#ifndef SYLE_FIRMWARE_IRQ_CR8_H
#define SYLE_FIRMWARE_IRQ_CR8_H
#include "firmware_irq_apic.h"
/* HOST-ONLY strict MOV RAX,CR8 (44 0f20 c0), fetched from owned executable
 * memory. No host CR8 access, EOI, accounting or TPR write. Snapshot/owned
 * memory stays immutable under exclusive stopped-vCPU ownership throughout.
 * Success preserves entire owner/TPR and zero-extends TPR[7:4] into RAX.
 * Validation rejection has no effects; callback failure/reentry permanently
 * quarantines session, never rolls back RF/RIP or retries. Native actual bytes
 * must be diagnosed and admitted separately; this does not assert run13 bytes.
 * Caller binds actual VMCS/session and verifies primary CR8-store-exiting
 * bit20=1, TPR-shadow bit21=0. This API cannot prove those external controls.
 * CR0/CR4 guest-host masks are unrelated; result is TPR, never ISR-derived PPR. */
enum fat_result fir8_read(struct fag_state*,struct ftis_session*,uint64_t[15],
 const struct fmm_memory*,const struct fat_exit*,unsigned length,bool exclusive,
 void*,lei_write);
#endif
