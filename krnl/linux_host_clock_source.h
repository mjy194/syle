#ifndef SYLE_LINUX_HOST_CLOCK_SOURCE_H
#define SYLE_LINUX_HOST_CLOCK_SOURCE_H
#include "linux_host_clock.h"
/* Decode admission facts only. CPUID does not prove suspend/live-migration
 * continuity; caller must separately own that experimental lifecycle contract. */
struct lhc_cpuid {
 uint32_t max_basic,max_extended,denominator,numerator,crystal_hz,power_edx;
};
bool lhc_frequency(const struct lhc_cpuid*,uint64_t*);
#endif
