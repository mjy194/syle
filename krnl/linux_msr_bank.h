#ifndef SYLE_LINUX_MSR_BANK_H
#define SYLE_LINUX_MSR_BANK_H
#include "linux_vcpu_state.h"
/* Guest-advertised CPUID bits, not raw host CPUID. FS_BASE is a guest VMCS
 * snapshot (field 0x680e), never the host MSR. Other accepted MSRs use the
 * EXISTING lvs_state.guest bank so SWAPGS/syscall state remains coherent.
 * Only CPL0 RDMSR/WRMSR, unprefixed len2, active long-mode profile.
 * STAR low32 must be zero: legacy-mode SYSCALL target is not implemented.
 * SFMASK high32 must be zero. LSTAR/CSTAR/FS/KERNEL_GS bases canonical48.
 * SCE need not be enabled to program syscall MSRs; CPUID SYSCALL is required.
 * Caller checks real exit, no vectoring/pending events, and current CPL.
 * Called after successful lvs host restoration, never concurrently/reentrantly.
 * Writes only guest VMCS and guest bank. No native MSR access or pass-through.
 * On false, bank/GPR stay unchanged; VMCS may have partially changed, so STOP
 * and do not resume. Rejected unsupported requests perform no VMCS writes.
 * Guest RAM/EPT/VMX ownership and exception injection belong to the caller.
 */
bool lmbr_apply(uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,
 unsigned length,uint32_t basic_edx,uint32_t extended_edx,uint64_t guest_fs,
 struct lvs_state *,void *,bool(*write_vmcs)(void*,uint32_t,uint64_t));
#endif
