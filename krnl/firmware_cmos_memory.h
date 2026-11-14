#ifndef SYLE_FIRMWARE_CMOS_MEMORY_H
#define SYLE_FIRMWARE_CMOS_MEMORY_H
#include "firmware_layout.h"
/* Read-only size facts for the fixed cold-boot firmware platform. This pure
 * model validates a layout, NOT allocation/ownership or native execution.
 * Caller must supply its prepared, nonquarantined firmware_memory.layout,
 * with owned backing verified, and keep platform facts frozen thereafter.
 * It never accesses host CMOS and does not implement RTC/NMI/shutdown state. */
struct fcmi_state { uint8_t above16_low,above16_high; bool ready; };
/* Exact canonical layout: contiguous RAM [0,128MiB), top-of-4GiB VARS/CODE,
 * RX CODE/R VARS, and their normal hole. Reject mutated region permissions,
 * kinds, overlaps, overflow, nonzero RAM base or any unsupported RAM size.
 * Fresh zeroed state required; failure leaves state unchanged. */
bool fcmi_init(struct fcmi_state*,const struct fwl_plan*);
/* 34/35: memory above16MiB in LE16 64KiB units; 5b/5c/5d: zero upper RAM.
 * Only these five indices accepted. No implicit index masking or unknown-zero
 * policy; IO layer separately admits index-port/NMI latch and read direction.
 * False leaves output unchanged. No data-write API. */
bool fcmi_read(const struct fcmi_state*,uint8_t index,uint8_t*out);
#endif
