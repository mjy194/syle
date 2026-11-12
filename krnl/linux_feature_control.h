#ifndef SYLE_LINUX_FEATURE_CONTROL_H
#define SYLE_LINUX_FEATURE_CONTROL_H
#include "linux_cpu_profile.h"
/* Locked guest-only disabled VMX/SMX/SGX policy. False means no resume. */
bool lfc_read(enum lcp_kind,const struct lvp_cpu_features*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned length,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
