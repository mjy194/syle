#ifndef SYLE_VMX_GUEST_H
#define SYLE_VMX_GUEST_H
#include <stdbool.h>
#include <stdint.h>
/* Trusted bounded diagnostic; shared host paging, no isolation. Caller owns
 * an initialized WB VMCS region and VMX root. Must run with interrupts off. */
/* Pure adjustment: fail if a desired bit is unavailable or a forced bit is
 * unsupported. Low half MSR = must-be-one, high half = may-be-one. */
static inline bool vmx_guest_controls(uint64_t caps, uint32_t want,
                                      uint32_t supported, uint32_t *out) {
    uint32_t required=(uint32_t)caps, allowed=(uint32_t)(caps>>32);
    uint32_t selected=want|required;
    if ((selected&~allowed) || (selected&~supported)) return false;
    *out=selected;
    return true;
}
static inline bool vmx_guest_exit_matches(uint64_t reason, uint64_t rip,
                                          uint64_t expected_rip, uint64_t length,
                                          uint64_t marker) {
    return reason==18 && rip==expected_rip && length==3 &&
           marker==UINT64_C(0x53594c45564d5831);
}
/* RSP/RIP/RFLAGS are hardware-managed VMCS fields; the other 15 GPRs
 * cross each VM exit through this assembly-visible save area. Single vCPU. */
struct vmx_guest_registers {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
};
enum vmx_dispatch_action { VMX_DISPATCH_REJECT, VMX_DISPATCH_RESUME, VMX_DISPATCH_DONE };
static inline enum vmx_dispatch_action vmx_guest_dispatch(
    unsigned phase, uint64_t reason, uint64_t rip, uint64_t expected_rip,
    uint64_t length, uint64_t rsp, uint64_t expected_rsp, uint64_t flags,
    const struct vmx_guest_registers *r) {
    if (phase >= 3 || reason != 18 || rip != expected_rip || length != 3 ||
        rip > UINT64_MAX - length || rsp != expected_rsp || flags != 0x47 ||
        r->rax != UINT64_C(0x53594c45564d5831) + phase || r->rbx != phase ||
        r->rcx != 0x103 || r->rdx != 0x104 || r->rsi != 0x105 ||
        r->rdi != 0x106 || r->rbp != 0x107 || r->r8 != 0x108 ||
        r->r9 != 0x109 || r->r10 != 0x10a || r->r11 != 0x10b ||
        r->r12 != 0x10c || r->r13 != 0x10d || r->r14 != 0x10e || r->r15 != 0x10f)
        return VMX_DISPATCH_REJECT;
    return phase == 2 ? VMX_DISPATCH_DONE : VMX_DISPATCH_RESUME;
}
bool vmx_guest_run(uint64_t vmcs_physical, uint64_t basic);
struct nested_vcpu;
bool vmx_guest_run_context(struct nested_vcpu *,uint64_t,uint64_t);
#endif
