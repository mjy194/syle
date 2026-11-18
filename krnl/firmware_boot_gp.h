#ifndef SYLE_FIRMWARE_BOOT_GP_H
#define SYLE_FIRMWARE_BOOT_GP_H
#include "firmware_mmio_memory.h"
struct fbg_state {uint64_t rip[8],cr3[8];unsigned count;bool pending,failed,busy;};
struct fbg_exit {uint64_t rip,rsp,idtr,idt_limit,gdtr,gdt_limit,flags;};
/* Only unsupported RDMSR CE. Caller authenticates actual CPL0 VM exit31,
 * length2, RCX, zero event metadata/vectoring/block/activity/pending debug,
 * disabled debug state and exclusive VMCS/owned RAM snapshot. Validates the
 * real instruction, IDT gate, GDT CS, executable handler and writable frame.
 * No RIP advancement: injects architectural #GP(0) for rdmsr_safe fixup. */
bool fbg_inject(struct fbg_state*,const struct fmm_memory*,const struct fbg_exit*,void*,bool(*)(void*,uint32_t,uint64_t));
/* Requires actual host-restored exit after the pending injection. Timer/EPT
 * and ambiguous receipts stop, rather than clearing and retrying an event. */
bool fbg_after(struct fbg_state*,uint32_t reason,uint32_t vectoring,uint32_t interruption,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
