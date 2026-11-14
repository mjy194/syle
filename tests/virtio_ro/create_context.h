#ifndef SYLE_VIRTIO_CREATE_CONTEXT_H
#define SYLE_VIRTIO_CREATE_CONTEXT_H
#include "native_candidate.h"
/* Per-CREATE PA callback compatibility experiment, not an installed memory-op
 * adapter. Caller storage and entries are disjoint, fresh-zero/stable and never reused
 * while handles survive. No host pointer mapping or DMA callbacks supplied. */
struct vtc_entry {uint64_t pa;struct vlm_allocation allocation;struct vlm_handle lease;enum vlm_kind kind;bool live,pinned,quarantined;};
struct vtc {struct vtc_entry *entries;unsigned count,capacity;uint64_t epoch;bool initialized,ended,failed,draining;};
bool vtc_open(struct vtc *,struct vtc_entry *,unsigned capacity);
bool vtc_allocate(struct vtc *,enum vlm_kind,uint64_t *pa);
bool vtc_pin_ram(struct vtc *,uint64_t pa);
bool vtc_unpin_ram(struct vtc *,uint64_t pa);
bool vtc_release(struct vtc *,uint64_t pa);
bool vtc_close(struct vtc *);
#endif
