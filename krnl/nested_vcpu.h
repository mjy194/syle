#ifndef SYLE_NESTED_VCPU_H
#define SYLE_NESTED_VCPU_H
#include "vmx_guest.h"
#include "ept.h"
#include <stddef.h>
struct nested_vcpu_frame {
    struct vmx_guest_registers regs;
    uint64_t guest_cr2,host_cr2;
};
#define NESTED_VCPU_OFFSET(member,n) _Static_assert(offsetof(struct nested_vcpu_frame,regs.member)==(n)*8,"nested frame " #member)
NESTED_VCPU_OFFSET(rax,0);NESTED_VCPU_OFFSET(rbx,1);NESTED_VCPU_OFFSET(rcx,2);
NESTED_VCPU_OFFSET(rdx,3);NESTED_VCPU_OFFSET(rsi,4);NESTED_VCPU_OFFSET(rdi,5);
NESTED_VCPU_OFFSET(rbp,6);NESTED_VCPU_OFFSET(r8,7);NESTED_VCPU_OFFSET(r9,8);
NESTED_VCPU_OFFSET(r10,9);NESTED_VCPU_OFFSET(r11,10);NESTED_VCPU_OFFSET(r12,11);
NESTED_VCPU_OFFSET(r13,12);NESTED_VCPU_OFFSET(r14,13);NESTED_VCPU_OFFSET(r15,14);
#undef NESTED_VCPU_OFFSET
_Static_assert(offsetof(struct nested_vcpu_frame,guest_cr2)==120,"nested guest CR2");
_Static_assert(offsetof(struct nested_vcpu_frame,host_cr2)==128,"nested host CR2");
_Static_assert(sizeof(struct nested_vcpu_frame)==136,"nested frame size");
struct nested_parallel_runtime;
struct nested_vcpu {
    struct nested_parallel_runtime *parallel;
    struct nested_vcpu_frame frame;
    struct ept_fixture fixture;
    uint32_t owner_apic;
    uint64_t vmcs01,vmcs02;
    bool launched01,launched02,busy,poison;
    unsigned last_status;
    uint64_t entries;
    uint64_t pending_vmcs,last_reason;
    bool cleared01,cleared02;
};
/* Pure, serialized owner-CPU lifecycle policy, no VMX instructions or memory
 * ownership inference. Fixture layout must use the SAME build configuration
 * in every translation unit. init is for fresh storage only.
 * Registered VMCS pages must already be caller-owned, initialized and cleared.
 * Hardware physical width/WB/capability checks remain the caller's duty. */
bool nested_vcpu_init(struct nested_vcpu *,uint32_t owner_apic,uint64_t vmcs01);
bool nested_vcpu_register02(struct nested_vcpu *,uint32_t actual_owner,uint64_t vmcs02);
/* Authorize before VMPTRLD/entry; record status on any subsequent failure.
 * resume must exactly match this slot's committed hardware launch state. */
bool nested_vcpu_authorize(struct nested_vcpu *,uint32_t actual_owner,
                           uint64_t expected_vmcs,bool resume);
/* entry_status=0 means assembly returned through VM-exit, not VMfail.
 * reason bit31 means VM-entry failure: not a successful launch/entry.
 * Failures consume busy and poison; prior launch state remains unchanged.
 * A VMREAD/dispatch failure must use nonzero entry_status, never reason=0 as
 * a substitute for unknown hardware outcome. Returns true only for true exit.
 * Owner/VMCS/busy mismatch is rejected without any state changes. */
bool nested_vcpu_complete(struct nested_vcpu *,uint32_t actual_owner,
    uint64_t expected_vmcs,unsigned entry_status,uint64_t exit_reason);
/* Call only AFTER successful local hardware VMCLEAR and never while busy.
 * Poison allows safe cleanup, but clear never removes poison. Slot addresses
 * stay registered so healthy cleared slots can VMLAUNCH again. */
bool nested_vcpu_clear(struct nested_vcpu *,uint32_t actual_owner,uint64_t vmcs);
#endif
