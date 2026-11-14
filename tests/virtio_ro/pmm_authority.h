#ifndef SYLE_HOST_PMM_AUTHORITY_H
#define SYLE_HOST_PMM_AUTHORITY_H
#include "lease_model.h"
#include "../../krnl/memory.h"
/* Optional host-link experiment against REAL memory.c. Single bootstrap CPU,
 * interrupts disabled; one managed memory session; no live guest integration.
 * No adopt API: every protected token is freshly allocated from PMM here. */
bool pmm_owned_alloc_typed(uint64_t owner,enum vlm_kind,struct vlm_allocation *);
bool pmm_owned_kind(uint64_t owner,struct vlm_allocation,enum vlm_kind *);
bool pmm_owned_alloc(uint64_t owner,struct vlm_allocation *);
bool pmm_owned_allocation_page(uint64_t owner,struct vlm_allocation,uint64_t *pa);
bool pmm_owned_pin(uint64_t owner,struct vlm_allocation,struct vlm_handle *);
bool pmm_owned_unpin(struct vlm_handle);
bool pmm_owned_release(uint64_t owner,struct vlm_allocation);
bool pmm_owned_lease_page(struct vlm_handle,uint64_t *pa);
void pmm_owned_begin_drain(void);
bool pmm_owned_drained(void);
#endif
