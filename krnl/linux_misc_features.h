#ifndef SYLE_MISC_FEATURES_H
#define SYLE_MISC_FEATURES_H
#include "linux_cpu_profile.h"
bool lmf_apply(enum lcp_kind,const struct lvp_cpu_features*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned length,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
