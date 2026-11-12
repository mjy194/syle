#ifndef SYLE_LINUX_CPU_COMMIT_H
#define SYLE_LINUX_CPU_COMMIT_H
#include "linux_vcpu_policy.h"
bool lcpu_host_valid(uint32_t max_basic,uint32_t vendor_b,uint32_t vendor_c,uint32_t vendor_d,uint32_t max_extended,const struct lvp_cpu_features *);
/* Caller verifies real exit/no pending vectoring. Failure: stop, no resume. */
bool lcpu_apply(uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned length,const struct lvp_cpu_features*,void*,bool (*write)(void*,uint32_t,uint64_t));
#endif
