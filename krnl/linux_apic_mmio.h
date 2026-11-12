#ifndef SYLE_LINUX_APIC_MMIO_H
#define SYLE_LINUX_APIC_MMIO_H
#include "linux_apic_msr.h"
#include "linux_mmio_memory.h"
/* One real EPT exit snapshot. VM-exit instruction length is NOT used. */
struct lamm_exit {
 uint64_t rip,rsp,qualification,linear,physical,ds_base,es_base,ss_base;
 uint32_t reason,vectoring;
};
/* Shares lams_state's EOI sequence/receipt with MSR transactions. Native GPR
 * array order AX BX CX DX SI DI BP R8..15; RSP comes from current VMCS snapshot.
 * Supports only the registered xAPIC page FEE00000, aligned 32-bit registers.
 * IOAPIC FEC00000 stays unsupported: no unsafe shallow callback clone.
 * Caller supplies true current VMCS snapshot, exclusive guest memory/device
 * ownership, no IRQ reservation/pending entry event, guest-visible APIC bit,
 * and trusted private-RAM reader. Failure forbids resume; no host APIC access.
 * RSP load writes RSP before RIP; partial VMWRITE is FATAL, no software commit.
 */
enum lams_result lamm_apply(struct lams_state*,uint64_t native_gpr[15],
 const struct lmm_memory*,const struct lamm_exit*,uint64_t now,
 uint32_t guest_edx,bool exclusive,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
