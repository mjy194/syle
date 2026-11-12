#ifndef SYLE_LINUX_VCPU_PREPARE_H
#define SYLE_LINUX_VCPU_PREPARE_H
#include "linux_guest_memory.h"
#define LVP_MAX_FIELDS 96u
struct lvp_caps {
 uint64_t pin,primary,secondary,entry,exit,ept;
 uint64_t cr0_fixed0,cr0_fixed1,cr4_fixed0,cr4_fixed1;
 unsigned physical_bits,timer_shift;
 bool want_wbinvd; /* Must be true; no executable downgrade profile. */
};
struct lvp_field {uint32_t encoding; uint64_t value;};
struct lvp_plan {
 struct lvp_field fields[LVP_MAX_FIELDS];
 unsigned count;
 uint64_t gpr[15]; /* vmx_guest_registers order; only RSI initially nonzero */
};
/* Pure, no VMX/host state writes. All input must remain immutable while building.
 * RAM ownership/uniqueness/WB and EPT contents are trusted lgm_prepare contracts.
 * Fixed 128MiB direct64 only. Output unchanged on rejection. Runtime must add
 * complete checked host state, reload preemption timer each entry, stop on any
 * partial VMWRITE failure and enforce its separate total-exit/TSC budget.
 * CR4.VMXE is hardware-only, not exposed by CR4 read shadow. No VPID.
 * TR: selector 0x20, base 0x6000, limit 103, busy 64-bit TSS. Loader must
 * supply its private zeroed TSS page (RSP0=0x80000, IST=0, I/O map base104),
 * and matching busy TSS GDT descriptor at 0x20; GDT limit47. This models
 * an already loaded TR, not an LTR operation. Task switching is unsupported.
 * All interrupts/exceptions initially exit; runtime must not inject an event
 * requiring an unconfigured IST or privilege transition. WBINVD exits required.
 * Caps are effective true controls when available, otherwise legacy controls.
 */
bool lvp_prepare(const struct linux_guest_memory*,const struct lvp_caps*,struct lvp_plan*);
#endif
