#ifndef SYLE_LINUX_SYSENTER_MSR_H
#define SYLE_LINUX_SYSENTER_MSR_H
#include <stdint.h>
#include <stdbool.h>
/* Guest VMCS is source-of-truth. False requires caller fail-stop, even if
 * a failing VMWRITE may already have changed the guest field or RIP. */
bool lse_apply(uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,
 unsigned length,uint32_t basic_edx,uint32_t ext_edx,
 void *read_ctx,bool(*read)(void*,uint32_t,uint64_t*),
 void *write_ctx,bool(*write)(void*,uint32_t,uint64_t));
#endif
