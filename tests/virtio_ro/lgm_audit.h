#ifndef SYLE_HOST_LGM_AUDIT_H
#define SYLE_HOST_LGM_AUDIT_H
#include "../../krnl/linux_guest_memory.h"
/* Read-only consistency audit, NOT DMA admission or a pin/ownership proof.
 * Caller freezes memory, allocator, EPT and all vCPUs for the whole call.
 * Operates on the real LGM ledger shape, no caller-created owner label.
 * Output token is diagnostic only, not a capability to map/copy memory.
 * Output storage must be external to the LGM/EPT snapshot (trusted API rule).
 * No proof of other EPT leaves, other VMs, or allocator sharing is provided. */
bool vro_lgm_audit_page(const struct linux_guest_memory *,uint64_t gpa,bool write,uint64_t *token);
#endif
