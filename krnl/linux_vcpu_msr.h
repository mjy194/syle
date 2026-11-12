#ifndef SYLE_LINUX_VCPU_MSR_H
#define SYLE_LINUX_VCPU_MSR_H
#include <stdbool.h>
#include <stdint.h>
struct lvm_gs_state {
 uint64_t rax,rcx,rdx,rip,gs_base;
 unsigned cpl;
};
struct lvm_gs_change {
 struct lvm_gs_state next;
 bool write_vmcs;
 uint32_t field; /* 0x6810 guest GS_BASE; NEVER WRMSR to host */
 uint64_t value;
};
/* Pure staged RDMSR(reason31)/WRMSR(reason32) for IA32_GS_BASE only.
 * Caller verifies real VM-exit (no entry-failure) and no pending/vectoring
 * event before use. CPL must come from guest architectural state, not host.
 * MSR index uses low32 ECX; WRMSR consumes low32 EDX:EAX. RDMSR zero-extends
 * each 32-bit result. Both existing and proposed GS base must be canonical48.
 * Exact length2 supported; prefixed instruction variants deliberately reject.
 * No host MSR access. Failure leaves out untouched; apply failure must stop.
 * This does NOT implement SWAPGS/KERNEL_GS_BASE or general MSR support. */
bool lvm_gs_emulate(const struct lvm_gs_state *,uint64_t exit_reason,
                    unsigned instruction_length,struct lvm_gs_change *);
#endif
