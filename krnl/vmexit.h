#ifndef SYLE_VMEXIT_H
#define SYLE_VMEXIT_H
#include "vmx_guest.h"
enum vmexit_action { VMEXIT_REJECT, VMEXIT_RESUME, VMEXIT_HALT };
/* Deliberately tiny synthetic CPU profile, not a host CPUID pass-through. */
void vmexit_cpuid(struct vmx_guest_registers *r);
/* Pure integer-state handler. Rejection leaves registers and next_rip intact.
 * HLT is terminal for this single-vCPU diagnostic; not a scheduler model. */
enum vmexit_action vmexit_dispatch(uint64_t reason,uint64_t rip,uint64_t length,
    uint64_t code_start,uint64_t code_end,struct vmx_guest_registers *r,uint64_t *next_rip);
#endif
