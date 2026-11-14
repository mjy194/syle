#ifndef SYLE_FIRMWARE_ACTIVE_CPU_DE_H
#define SYLE_FIRMWARE_ACTIVE_CPU_DE_H
#include "firmware_active_cpu.h"
/* Independent DE exposure profile. Host DE required; caller must also enforce
 * fcmd debug/VMCS admission. This query does not enable debug virtualization. */
bool fcad_supported(const struct fcp_host*);
bool fcad_query(const struct fcp_host*,uint32_t,uint32_t,struct fcp_result*);
#endif
