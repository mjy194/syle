#ifndef SYLE_FIRMWARE_IRQ_LVT_SAME_H
#define SYLE_FIRMWARE_IRQ_LVT_SAME_H
#include "firmware_irq_apic.h"
/* Independent fixed timer profile: only MOV32 [FEE00320],20020 idempotence.
 * Legacy device.timer is not live timing state. Owner fixed vector32 periodic
 * 10M/div1 supplies that contract; caller samples its live clock BEFORE call.
 * No mask/reconfigure/reload implementation; all other values reject. Actual
 * run18 value remains unknown: only runtime owned decode establishes match.
 * Exclusive stable owner/mode/mappings/bytes required; no clock snapshot restore.
 * RF->RIP completion changes no device/owner/GPR fields except transient busy.
 * Partial callback/reentry quarantines session; no rollback or retry.
 */
enum fat_result fils_write(struct fag_state*,struct ftis_session*,uint64_t[15],
 const struct fmm_memory*,const struct fat_exit*,bool exclusive,void*,lei_write);
#endif
