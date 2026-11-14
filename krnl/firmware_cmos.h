#ifndef SYLE_FIRMWARE_CMOS_H
#define SYLE_FIRMWARE_CMOS_H
#include "linux_rtc.h"
#include "firmware_cmos_memory.h"
/* Reuses RTC synthetic epoch 2000-01-01, not real host wall time. Same admitted
 * Hv 10MHz source as APIC; exact rational conversion to RTC's 1193182Hz unit.
 * Compile RTC object with LRTC_FIRMWARE_COLD_BOOT for zero shutdown byte0F with idempotent clear only.
 * Init requires the canonical 128MiB layout from successful owned preparation;
 * size bytes34/35=00/07 and5B..5D=0 do not describe usable legacy holes.
 * Private NMI mask latch has no NMI source/delivery and never touches host. */
struct fcr_state {struct lrtc_state rtc;struct fcmi_state memory;uint64_t origin,last,epoch;bool ready,failed;};
bool fcr_init(struct fcr_state*,const struct fwl_plan*,uint64_t epoch,uint64_t reference);
bool fcr_io(struct fcr_state*,uint64_t[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t qualification,unsigned length,uint64_t epoch,uint64_t reference,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
