#ifndef SYLE_LINUX_VCPU_BACKEND_H
#define SYLE_LINUX_VCPU_BACKEND_H
#include "linux_vcpu_host.h"
struct lvb_context { unsigned physical_bits; bool bound,active; uint64_t root,current; };
/* Trusted pinned BSP, CPL0, IF/DF clear, no existing VMX owner. No AP started.
 * Bind does not allocate, enable VMXE, or execute VMX. Caller enables VMXE
 * before lvh_open and restores original CR4 only AFTER successful lvh_close.
 * All callbacks and descriptor storage remain resident/immutable; GDT/TSS
 * belong to the kernel. Debug GD must already be clear before capture.
 * Native exact MSR #GP/#UD hook must be installed. VMX instructions rely on
 * checked architectural preconditions; unexpected faults are fatal, never
 * converted to success or ownership release. CF/ZF failures retain state.
 * All reachable code must be general-registers-only, no-red-zone.
 */
bool lvb_bind(struct lvb_context *,const struct sg_memory_ops *,struct lvh_ops *);
/* Pure descriptor decoder for validation/tests. limit is inclusive, bytes
 * is the actual readable byte count; does not dereference hardware pointers. */
bool lvb_descriptor(const uint8_t *,size_t,uint16_t,uint16_t,
                    uint32_t *,uint32_t *,uint64_t *);
#endif
