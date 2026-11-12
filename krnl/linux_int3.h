#ifndef SYLE_INT3_H
#define SYLE_INT3_H
#include "linux_nonpresent_pf.h"
#define LBP_MAX_FAULTS 8u
struct lbp_tuple{uint64_t rip,cr3;};
struct lbp_state{struct lbp_tuple history[LBP_MAX_FAULTS];unsigned count;bool pending,failed;};
bool lbp_observe(const struct lbp_state*,uint64_t reason,uint64_t vector,uint64_t event,struct lbp_state*);
bool lbp_inject(struct lbp_state*,struct lgs_session*);
#endif
