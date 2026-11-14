#ifndef SYLE_FIRMWARE_CMOS_INDEX_READ_H
#define SYLE_FIRMWARE_CMOS_INDEX_READ_H
#include "firmware_cmos.h"
#include "firmware_apic_transaction.h"
#include "linux_external_irq.h"
/* Explicit Syle virtual port70 latch readback; not universal chipset behavior.
 * No host ports/NMI delivery or RTC time/status advancement. Native supplies
 * immutable bytes fetched from owned executable memory and proves long mode,
 * actual VMCS/CPL, exclusive ownership and same-session latch provenance.
 * Exact EC / IN AL,DX only, DX70, q700008, length1. No immediate/string forms.
 * Fresh private guard; failures after VMwrite/reentry permanently latch guard
 * and fcr.failed; caller must stop the whole session, no rollback/retry.
 */
struct fcri_guard {bool busy,failed;};
enum fat_result fcri_read(struct fcri_guard*,struct fcr_state*,uint64_t[15],
 const struct fat_exit*,unsigned length,unsigned cpl,const uint8_t *owned_bytes,
 size_t available,bool exclusive,void*,lei_write);
#endif
