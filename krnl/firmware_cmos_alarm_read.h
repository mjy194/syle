#ifndef SYLE_FIRMWARE_CMOS_ALARM_READ_H
#define SYLE_FIRMWARE_CMOS_ALARM_READ_H
#include "firmware_cmos.h"
#include "firmware_apic_transaction.h"
#include "linux_external_irq.h"
/* Explicit virtual cold alarm bytes1/3/5 all zero, read-only; NOT host alarm
 * or a claim about unspecified hardware reset values. Existing RTC B=02
 * keeps AIE=0; no AF/alarm producer/delivery. Alarm writes or B-enable remain
 * unsupported. Caller freezes this cold profile, proves owned bytes/longmode/
 * actual VMCS/CPL and exclusive same-session state. Exact EC/DX71/q710008/len1.
 * AL zero, upper56 preserved; entire RTC state unchanged, no tick/status reads
 * or clears. RF->RIP->AL; callback failure/reentry permanently poison guard and
 * fcr. No rollback/retry, native caller must stop whole session on failure.
 */
struct fcra_guard {bool busy,failed;};
enum fat_result fcra_read(struct fcra_guard*,struct fcr_state*,uint64_t[15],
 const struct fat_exit*,unsigned length,unsigned cpl,const uint8_t *owned_bytes,
 size_t available,bool exclusive,void*,lei_write);
#endif
