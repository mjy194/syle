#ifndef SYLE_DEFERRED_CREATE_CONTEXT_EXPERIMENT_H
#define SYLE_DEFERRED_CREATE_CONTEXT_EXPERIMENT_H
#include "native_candidate.h"
/* Separate exploration; no frozen create_context code changed. Logical retire
 * does NOT return memory to PMM. Finalize must run and report retained failures.
 * Not a drop-in sg_memory_ops.release; no real mapping or DMA callback. */
struct vdc_entry {uint64_t pa;struct vlm_allocation allocation;struct vlm_handle lease;enum vlm_kind kind;bool present,retired,pinned;};
struct vdc {struct vdc_entry *entries;unsigned count,budget;uint64_t epoch;bool opened,draining,ended,failed;};
bool vdc_open(struct vdc *,struct vdc_entry *,unsigned lifetime_budget);
bool vdc_allocate(struct vdc *,enum vlm_kind,uint64_t *);
bool vdc_pin_ram(struct vdc *,uint64_t);
bool vdc_unpin(struct vdc *,uint64_t);
bool vdc_retire(struct vdc *,uint64_t);
/* PRECONDITION before the FIRST call: controller proves all guest execution,
 * device writers and EPT/VMX/dependent-resource uses globally quiesced. This
 * function does NOT establish that barrier. It may free unpinned entries while
 * other entries remain pinned, and false can mean partial physical release.
 * Retain context for cleanup; never resume/reuse partially released resources. */
bool vdc_finalize(struct vdc *);
#endif
