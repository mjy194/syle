#ifndef SYLE_FIRMWARE_ACTIVE_CPU_H
#define SYLE_FIRMWARE_ACTIVE_CPU_H
#include "firmware_cpu_profile.h"
bool fca_supported(const struct fcp_host*);
bool fca_query(const struct fcp_host*,uint32_t,uint32_t,struct fcp_result*);
#endif
