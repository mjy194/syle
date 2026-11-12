#ifndef SYLE_SMP_VMX_H
#define SYLE_SMP_VMX_H
#include <stdbool.h>
#include <stdint.h>
#include "vmx.h"
enum smp_vmx_status {
    SMP_VMX_OK, SMP_VMX_BAD_CONTEXT, SMP_VMX_WRONG_CPU,
    SMP_VMX_BLOCKED_NO_VMX, SMP_VMX_BLOCKED_FEATURE_CONTROL,
    SMP_VMX_BLOCKED_BASIC, SMP_VMX_BLOCKED_ADDRESS,
    SMP_VMX_BLOCKED_CR0, SMP_VMX_BLOCKED_CR4, SMP_VMX_BLOCKED_WB,
    SMP_VMX_ON_FAILED, SMP_VMX_PTR_FAILED, SMP_VMX_CURRENT_VMCS,
    SMP_VMX_OFF_FAILED, SMP_VMX_RESTORE_FAILED, SMP_VMX_BLOCKED_IF
};
enum smp_vmx_phase { SMP_VMX_READY, SMP_VMX_ENTERED, SMP_VMX_CHECKED,
                     SMP_VMX_EXITED, SMP_VMX_FAILED };
struct smp_vmx_context {
    uint64_t region_hpa;
    void *region_view;
    uint64_t saved_cr0, saved_cr4, last_vmcs;
    uint32_t owner_apic_id;
    enum smp_vmx_phase phase;
    enum smp_vmx_status status;
    bool root_live;
};
/* Pure policy. On success only CR0.NE may be added, and only when required. */
enum smp_vmx_status smp_vmx_policy(const struct vmx_caps *caps,
    uint64_t region_hpa, unsigned physical_bits, uint64_t *proposed_cr0);
/* BSP supplies zeroed context and unique allocator-owned mapped WB RAM page.
 * region_view covers an aligned writable 4KiB mapping of exactly region_hpa,
 * PAT index 0. Caller guarantees no alias with another CPU's region.
 * Local CPU only, serialized, IF=0; no VMCS operations or allocations.
 * root_live requires retaining context/page even after check/leave failures.
 * Do not retry enter; recreate context only after safe cleanup. */
enum smp_vmx_status smp_vmx_local_enter(struct smp_vmx_context *ctx);
enum smp_vmx_status smp_vmx_local_check(struct smp_vmx_context *ctx);
enum smp_vmx_status smp_vmx_local_leave(struct smp_vmx_context *ctx);
#endif
