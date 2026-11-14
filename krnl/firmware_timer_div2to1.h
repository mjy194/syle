#ifndef SYLE_FIRMWARE_TIMER_DIV2TO1_H
#define SYLE_FIRMWARE_TIMER_DIV2TO1_H
#include "firmware_timer_100m_mmio.h"
/* Explicit opt-in virtual-platform experiment, not Intel/QEMU equivalence.
 * Exact observed masked periodic initialFFFFFFFF/LVT30005/DCR0/div2 -> DCRB/div1.
 * 10MHz reference yields100ns observation granularity, NOT10ns sampling.
 * First sample OLD state; due/terminal/clock overflow precedence preserved.
 * Only then preserve current count/phase via exact even remaining_bus/2.
 * No initial reload, no restart, no general divider support. Invalid shapes
 * reject after sampling; caller must retain sample effects. Disabled APIC or
 * arithmetic overflow is terminal. Output is caller's stage; native owner must
 * live-sample first at SAME reference, clone, call, publish only after VMCS
 * success. Partial commit permanently stops; never restore old live clock.
 * Default ft100_mmio_access armed-divider rejection remains unchanged. */
enum ft100_result ft100_mmio_redivide_2_to_1(struct ft100_mmio*,uint64_t epoch,
 uint64_t reference,bool enabled,unsigned width,uint32_t value);
#endif
