#ifndef SYLE_LINUX_HOST_CLOCK_HV_H
#define SYLE_LINUX_HOST_CLOCK_HV_H
#include "linux_host_clock_runtime.h"
struct lhch_ops {void*ctx;bool(*cpuid)(void*,uint32_t,uint32_t[4]);bool(*reference)(void*,uint64_t*);};
/* Explicit Hyper-V interface profile only; never a physical-host fallback. */
struct lhch_context {struct lhch_ops ops;uint32_t cpu;uint64_t epoch;bool admitted;};
bool lhch_admit(struct lhch_context*,const struct lhch_ops*,uint64_t,struct lhcr_source*);
bool linux_host_clock_native_source(struct lhcr_source*);
#endif
