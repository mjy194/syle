#ifndef SYLE_SCOPED_ACCESS_H
#define SYLE_SCOPED_ACCESS_H
#include "session_authority.h"
#include <stddef.h>
/* Host-only bounded-operation pilot. No owned pointer is returned. Backend
 * transfer must be synchronous, cannot retain buffer/PA or start async work.
 * false may mean partial copy, but MUST mean execution finished. Trusted
 * host_buffer verifies external spans/alias rules before any transfer.
 * Native paging/NMI/device exclusion is not supplied by these callbacks. */
struct vsc_ops {
 void *context;
 bool (*host_buffer)(void *,const void *,size_t);
 bool (*transfer)(void *,uint64_t,size_t,void *,size_t,bool);
};
struct vsc {
 struct vsa *authority;
 struct vsc_ops ops;
 uint64_t epoch;
 unsigned borrowers;
 bool initialized,busy,closing,faulted,ended;
};
bool vsc_init(struct vsc *,struct vsa *,const struct vsc_ops *);
bool vsc_allocate(struct vsc *,enum sg_memory_role,struct vlm_allocation *);
bool vsc_copy(struct vsc *,struct vlm_allocation,enum vlm_kind,size_t,void *,size_t,bool to_owned);
bool vsc_drain(struct vsc *);
/* Only from proven global close barrier; never an admission of VMX/DMA safety.
 * Logical revocation rejects new calls. Previously escaped raw map pointers
 * are incompatible with this ownership contract. Failure retains records. */
bool vsc_retire_after_quiescence(struct vsc *);
#endif
