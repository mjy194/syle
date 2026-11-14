#ifndef SYLE_FIRMWARE_CMOS_VALID_WRITE_H
#define SYLE_FIRMWARE_CMOS_VALID_WRITE_H
#include "firmware_cmos.h"
#include "firmware_apic_transaction.h"
#include "linux_external_irq.h"
/* Independent virtual RTC read-only registerD ignored-zero-write contract.
 * Exact owned EE/DX71/q710000/len1, existing indexD and AL0 only. This does NOT
 * clear VRT: existing synthetic registerD reads80 before and after. No battery
 * state/hardware claim, no time/status/NMI/rate/calendar changes. Fixed EDK2
 * PcRtc.h labels D read-only, PcRtc.c writes PCD default0 during init. Unknown
 * writes remain fail-stop; no general CMOS write support is granted.
 * Caller proves longmode/actualVMCS/CPL and immutable owned bytes/latch under
 * exclusivity. RF->RIP only; success preserves entire fcr and every GPR.
 * Partial callback/reentry permanently poison guard+fcr; no retry/rollback.
 */
struct fcrv_guard {bool busy,failed;};
enum fat_result fcrv_write(struct fcrv_guard*,struct fcr_state*,uint64_t[15],
 const struct fat_exit*,unsigned length,unsigned cpl,const uint8_t *owned_bytes,
 size_t available,bool exclusive,void*,lei_write);
#endif
