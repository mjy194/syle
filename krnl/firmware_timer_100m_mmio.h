#ifndef SYLE_FIRMWARE_TIMER_100M_MMIO_H
#define SYLE_FIRMWARE_TIMER_100M_MMIO_H
#include "firmware_timer_100m.h"
/* NONDEFAULT, NON-HARD-REALTIME profile: virtual bus 100MHz, Hv reference
 * 10MHz. Finite guest execution slices do not establish a wall-time bound.
 * Owner synchronizes at exit, before entry, and on each MMIO. No IRQ injection.
 * Any unmasked expiry (including vector<16) remains pending as terminal STOP.
 * Initialize fresh zeroed private state, serialize every operation. */
struct ft100_mmio { struct ft100 timer; uint32_t dcr; };
bool ft100_mmio_init(struct ft100_mmio*,uint64_t epoch,uint64_t reference,uint64_t max_gap);
/* All attempted accesses sample OLD time/state before validation/configuration.
 * BAD may advance last/consume MASKED expiries; never snapshot-roll back this
 * state, including on failed subsequent VMCS/GPR commit. STOP/terminal is sticky.
 * Reads publish output only on OK. Bad writes preserve configuration except
 * sampled time effects. Valid writes commit immediately: caller must fail-stop
 * if its subsequent architectural commit fails, not retry or roll back.
 * software_enabled is current SVR enable. Disabled means terminal BAD after
 * sampling; it never permits timer progress/resume silently without delivery.
 * Supported offsets:320 LVT,380 initial,390 current (RO),3e0 divide; width=4.
 * Armed divisor writes and armed periodic-mode changes reject conservatively.
 * Mask/vector writes preserve deadline/phase. No vector>=16 restriction until
 * delivery: low vectors may be configured but expiry always stops this profile.
 */
enum ft100_result ft100_mmio_access(struct ft100_mmio*,uint64_t epoch,uint64_t reference,
 bool software_enabled,uint64_t offset,unsigned width,bool write,uint32_t *value);
#endif
