#ifndef SYLE_LINUX_VCPU_HOST_H
#define SYLE_LINUX_VCPU_HOST_H
#include "linux_vcpu_prepare.h"
struct lvh_host {
 uint64_t cr0,cr3,cr4,flags,fs_base,gs_base,tr_base,gdt_base,idt_base;
 uint64_t pat,efer,sysenter_esp,sysenter_eip,debugctl,dr7;
 uint32_t sysenter_cs,tr_access,tr_limit;
 uint16_t es,cs,ss,ds,fs,gs,tr,gdt_limit,idt_limit;
};
/* Callback model, not a native privileged implementation. capture must inspect
 * valid resident GDT/TSS descriptors and report the exact same pinned CPU.
 * Memory allocator guarantees distinct exclusively owned pages, disjoint from
 * guest RAM/EPT and other kernel objects; callbacks/context remain alive.
 * Native entry backend must restore original GDT/IDT limits before C return.
 * Caller supplies verified Intel VMX CPUID and physical_bits, enabled VMXE,
 * ring0 IRQ/DF clear, no other live VMX session, and stable inputs/callbacks.
 * read_msr must contain #GP/#UD. vmxon/vmptrld failure means no state change;
 * vmxoff false MUST mean CPU remains in VMX operation, with no successful
 * VMXOFF hidden by a later check. Unknown completion is not representable:
 * backend must fatal-stop/reconcile, never return false then blindly retry.
 * All other failures retain ownership until explicitly retried. Backend must
 * preserve host architectural state on every entry before C use (not here).
 */
struct lvh_ops {
 void *context;
 struct sg_memory_ops memory;
 unsigned physical_bits;
 bool intel_vmx;
 bool (*capture)(void*,struct lvh_host*);
 bool (*read_msr)(void*,uint32_t,uint64_t*);
 bool (*vmxon)(void*,uint64_t);
 bool (*vmclear)(void*,uint64_t);
 bool (*vmptrld)(void*,uint64_t);
 bool (*vmwrite)(void*,uint32_t,uint64_t);
 bool (*invept)(void*,uint64_t);
 bool (*vmxoff)(void*);
};
/* Zero-init, large object: static/heap only. Never release guest RAM until
 * lvh_close succeeds. An unsuccessful close is a hard lifecycle blocker.
 * Cross-owner/duplicate allocation sets quarantined before mapping/zeroing.
 * Quarantined tokens are never automatically freed: allocator ownership must
 * be reconciled externally before reuse; open/close remain rejected. */
struct lvh_context {
 struct lvh_ops owner;
 struct lvh_host host;
 struct lvp_caps caps;
 struct lvp_plan plan;
 uint64_t basic,vmxon_page,vmcs_page,eptp;
 bool active,ready,quarantined;
 /* CREATE diagnosis: 1 guard,2 caps,3 capture,4 plan,5 pages,6 VMXON,
  * 7 VMCLEAR/VMPTRLD,8 guest writes,9 host writes,10 ready. */
 unsigned open_stage;
};
bool lvh_open(struct lvh_context*,const struct lvh_ops*,const struct linux_guest_memory*,uint64_t exit_rsp,uint64_t exit_rip);
bool lvh_close(struct lvh_context*);
#endif
