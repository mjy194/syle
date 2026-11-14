#ifndef SYLE_OPTIONAL_PMM_NATIVE_CANDIDATE_H
#define SYLE_OPTIONAL_PMM_NATIVE_CANDIDATE_H
#include "lease_model.h"
#include "../../krnl/memory.h"
/* Optional replacement PMM object. No native guest integration yet.
 * BSP + interrupts disabled only. Session IDs are kernel-internal handles,
 * never guest-controlled credentials. One session, one lease/page maximum. */
unsigned pna_capacity(void);
bool pna_start(uint64_t *epoch);
bool pna_alloc_typed(uint64_t epoch,enum vlm_kind,struct vlm_allocation *);
bool pna_kind(uint64_t epoch,struct vlm_allocation,enum vlm_kind *);
bool pna_alloc(uint64_t epoch,struct vlm_allocation *);
bool pna_page(uint64_t epoch,struct vlm_allocation,uint64_t *);
bool pna_pin(uint64_t epoch,struct vlm_allocation,struct vlm_handle *);
bool pna_unpin(uint64_t epoch,struct vlm_handle);
bool pna_release(uint64_t epoch,struct vlm_allocation);
bool pna_drain(uint64_t epoch);
bool pna_end(uint64_t epoch);
#ifdef PNA_HOST_TEST
void pna_test_guard(bool);
bool pna_test_exhaust_epoch(void);
#endif
#endif
