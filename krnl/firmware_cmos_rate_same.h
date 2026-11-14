#ifndef SYLE_FIRMWARE_CMOS_RATE_SAME_H
#define SYLE_FIRMWARE_CMOS_RATE_SAME_H
#include "firmware_cmos.h"
#include "firmware_apic_transaction.h"
#include "linux_external_irq.h"
/* Independent fixed virtual RTC registerA same-value write only. Runtime
 * owned opcode EE/DX71/q710000/len1, existing indexA and AL26 required. Other
 * register writes (including D) are NOT admitted. AL26 means divider010 and
 * rate0110: conventional 32.768kHz base / 1024Hz periodic rate. This synthetic
 * RTC already fixes registerA to26 (plus read-only UIP); no divider reset,
 * rate/phase change, host CMOS access, time/status advancement or IRQ action.
 * Caller proves longmode/actualVMCS/CPL and immutable owned bytes/latch under
 * exclusivity. RF->RIP only; success preserves entire fcr and every GPR.
 * Partial callback/reentry permanently poison guard+fcr; no retry/rollback.
 */
struct fcrs_guard {bool busy,failed;};
enum fat_result fcrs_write(struct fcrs_guard*,struct fcr_state*,uint64_t[15],
 const struct fat_exit*,unsigned length,unsigned cpl,const uint8_t *owned_bytes,
 size_t available,bool exclusive,void*,lei_write);
#endif
