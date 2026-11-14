#ifndef SYLE_FIRMWARE_ACTIVE_CPU_TSC_H
#define SYLE_FIRMWARE_ACTIVE_CPU_TSC_H
#include "firmware_active_cpu_de.h"
/* Quiet opt-in virtual-TSC feature. Host TSC + admitted reference required.
 * Adds only TSC bit4 atop DE profile, no RDTSCP/invariant/leaf15 capability.
 * Native must enable RDTSC exits and route the fixed-frequency virtual model.
 * MSR10 remains unsupported fail-stop; not a complete TSC implementation. */
bool fcats_supported(const struct fcp_host*);
bool fcats_query(const struct fcp_host*,bool source_admitted,uint32_t,uint32_t,struct fcp_result*);
#endif
