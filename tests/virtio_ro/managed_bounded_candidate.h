#ifndef SYLE_MANAGED_BOUNDED_CANDIDATE_H
#define SYLE_MANAGED_BOUNDED_CANDIDATE_H
#include "linux_guest_role_binding.h"
#include "lgm_bounded_candidate.h"
/* Isolated fixed-path adaptation of actual sidecar, never default selection. */
bool mbp_configure(struct lgr_binding*,struct lgs_session*,void*,sg_allocate_role_fn,bool(*)(void*),bool(*)(void*));
bool mbp_configure_barrier(struct lgr_binding*,struct lgs_session*,void*,sg_allocate_role_fn,bool(*)(void*),bool(*)(void*),bool(*)(void*),void*);
extern const struct guest_backend mbp_guest_backend;
bool mbp_host_open(struct lvh_context*,const struct lvh_ops*,const struct linux_guest_memory*,uint64_t,uint64_t,sg_allocate_role_fn);
bool mbp_prepare_done(void*);
#endif
