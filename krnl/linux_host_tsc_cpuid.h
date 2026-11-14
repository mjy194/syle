#ifndef SYLE_LINUX_HOST_TSC_CPUID_H
#define SYLE_LINUX_HOST_TSC_CPUID_H
#include "linux_vcpu_policy.h"
/* Only for admitted host-clock PIC04 profile, single vCPU. Virtual TSC uses
 * 1000 * PIT-domain tick, not physical host TSC. No RDTSCP/deadline advertisement. */
bool lht_query(const struct lvp_cpu_features*,uint32_t,uint32_t,struct lvp_cpuid_result*);
bool lht_apply(const struct lvp_cpu_features*,uint64_t[15],uint64_t,unsigned,unsigned,unsigned,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
