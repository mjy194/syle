#ifndef SYLE_FIRMWARE_VCPU_HOST_H
#define SYLE_FIRMWARE_VCPU_HOST_H
#include "linux_vcpu_host.h"
#include "firmware_vcpu_plan.h"
#include "firmware_memory.h"
/* Separate reset firmware opener; Linux opener is unchanged. All lvh_ops
 * ownership, pinning, failure and native host-state contracts also apply.
 * Close with lvh_close; release firmware backing only after successful close. */
bool fwh_open(struct lvh_context*,const struct lvh_ops*,const struct firmware_memory*,uint64_t,uint64_t);
#endif
