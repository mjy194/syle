#ifndef SYLE_MSR_GP_H
#define SYLE_MSR_GP_H
#include "linux_nonpresent_pf.h"
#define LGP_MAX_FAULTS 8u
struct lgp_tuple{uint64_t rip,cr3;};
struct lgp_state{struct lgp_tuple history[LGP_MAX_FAULTS];unsigned count;bool pending,failed;};
bool lgp_observe(const struct lgp_state*,uint64_t reason,uint64_t vector,uint64_t event,struct lgp_state*);
bool lgp_inject(struct lgp_state*,struct lgs_session*);
#endif
