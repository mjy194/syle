#ifndef SYLE_LINUX_GUEST_ROLE_BINDING_H
#define SYLE_LINUX_GUEST_ROLE_BINDING_H
#include "linux_guest_session.h"
/* Opt-in trusted sidecar, NOT an ownership proof or DMA adapter. Fresh zero
 * storage; stable address; exclusively controller-owned before publication.
 * No direct session backend calls, concurrent callers, callback reentry, or
 * mutation of session ops/context/binding while installed. begin/end implement
 * the real allocator epoch, not an untrusted owner label. begin failure may
 * retain resources: end must handle that state. end false retains retry state.
 * Real VMX close and RAM release must succeed BEFORE end can reclaim epoch
 * metadata. Neither this interface nor STOP proves NMI/device quiescence. */
struct lgr_binding {
 struct lgs_session *session;
 void *context;
 sg_allocate_role_fn allocate_role;
 bool (*begin)(void *),(*end)(void *);
 bool configured,active,ready,base_released;
 bool (*close_barrier)(void *);
 void *barrier_context;
 bool close_inflight;
};
bool lgr_configure(struct lgr_binding *,struct lgs_session *,void *,
 sg_allocate_role_fn,bool (*)(void *),bool (*)(void *));
/* Configure only before publication. Barrier belongs to this session/epoch;
 * must validate its private lease/borrower state, be idempotent, and never
 * restore CR4/free pages. No native DMA exclusion is supplied here. */
bool lgr_configure_barrier(struct lgr_binding *,struct lgs_session *,void *,
 sg_allocate_role_fn,bool (*)(void *),bool (*)(void *),bool (*)(void *),void *);
extern const struct guest_backend lgr_guest_backend;
#endif
