#ifndef SYLE_LINUX_DISABLED_APIC_H
#define SYLE_LINUX_DISABLED_APIC_H
#include "linux_cpu_profile.h"
/* Baseline represents an integrated virtual APIC kept globally disabled, not
 * absence of this MSR. Fixed base+BSP, no enable/x2 bits. Read only subset;
 * writes fail-stop, not a synthesized architectural #GP or silent success. */
bool lda_read(enum lcp_kind,uint32_t ecx,uint32_t edx,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint32_t reason,unsigned len,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
