#ifndef SYLE_LINUX_VCPU_NATIVE_H
#define SYLE_LINUX_VCPU_NATIVE_H
#include "linux_vcpu_state.h"
struct lvn_context {
 uint64_t gpr[15]; /* AX BX CX DX SI DI BP R8..R15; RSP/RIP/RFLAGS in VMCS */
 uint64_t cr2,host_cr2;
 uint32_t resume,entry_status; /* 0 VMfail, 1 exit, 2 host-VMWRITE failure */
 bool enabled;
 _Alignas(8) uint64_t host_dr6;
 uint8_t host_gdtr[10],host_idtr[10];
};
/* Exclusive pinned BSP, IF=0, no NMI xstate/MSR use, resident trusted buffers.
 * Compile all reachable C with general-registers-only/no-red-zone. No tracing.
 * Caller owns VMX root, current exclusive VMCS, full host VMCS validation,
 * and guest policy denying XSAVE/AVX/PKU/CET/AMX/unpreserved state.
 * bind neither enables VMX nor installs handlers. The opt-in exception hook
 * must already be present. Debug registers inactive; host CR0.EM/TS clear.
 * No automatic resume transition: caller sets resume=1 after real valid exit.
 * enter=false means no guest instructions ran in that invocation. VM-entry
 * failure delivered as VM exit returns true; caller must inspect exit reason.
 */
bool lvn_bind(struct lvn_context *,struct lvs_ops *);
bool lvn_enter(void *);
bool lvn_read_msr(void *,uint32_t,uint64_t *);
bool lvn_write_msr(void *,uint32_t,uint64_t);
/* Dispatcher must check saved CS CPL0; only exact armed #GP/#UD sites match. */
bool lvn_fault_fixup(unsigned,uint64_t *);
extern volatile uint64_t lvn_armed_rip;
#endif
