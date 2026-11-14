#ifndef SYLE_FIRMWARE_IRQ_CR8_WRITE_H
#define SYLE_FIRMWARE_IRQ_CR8_WRITE_H
#include "firmware_irq_apic.h"
/* Independent host-only exact MOV CR8,RAX (44 0f22 c0), reason28/q8/len4.
 * Input RAX must be <=15; unsupported #GP cases reject without effects, not
 * fake exception injection. Successful write sets TPR=RAX<<4: low nibble ZERO
 * (Intel Vol3A11.8.6.1), not preserved. GPR/entire IRQ owner remain unchanged.
 * Caller binds actual VMCS/session, exclusive stopped CPU/immutable owned
 * fetch, CR8-load-exiting primary19=1 and TPR-shadow21=0. No host CR8 access.
 * RF then RIP commit precedes TPR publish. Partial writes/reentry permanently
 * quarantine; no rollback, retry, EOI, account or pending consumption.
 */
enum fat_result fir8_write(struct fag_state*,struct ftis_session*,uint64_t[15],
 const struct fmm_memory*,const struct fat_exit*,unsigned length,bool exclusive,
 void*,lei_write);
#endif
