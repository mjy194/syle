#ifndef SYLE_LINUX_GS_COMMIT_H
#define SYLE_LINUX_GS_COMMIT_H
#include "linux_vcpu_msr.h"
/* Failure may have changed VMCS; caller must stop and never resume. */
bool lgsc_apply(uint64_t gpr[15],uint64_t rip,uint64_t gs,unsigned cpl,
 uint64_t reason,unsigned length,void *ctx,
 bool (*write)(void*,uint32_t,uint64_t));
#endif
