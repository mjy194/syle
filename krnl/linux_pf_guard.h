#ifndef SYLE_LINUX_PF_GUARD_H
#define SYLE_LINUX_PF_GUARD_H
#include "linux_vcpu_event.h"
struct lpfg_state {unsigned count,seen;bool pending,closed;};
/* Pure staged transitions. Commit observe output only after clearing consumed
 * entry-info in VMCS; commit inject output only after all injection writes.
 * Conservative diagnostic fence: any different RIP after the first PF
 * permanently disables injection, even a handler CR exit; may stop early.
 * A VM-entry failure never consumes a pending event. No fallback or rearming. */
bool lpfg_observe(const struct lpfg_state*,uint64_t,uint64_t,uint64_t,uint64_t,struct lpfg_state*);
bool lpfg_inject(const struct lpfg_state*,const struct lve_pf_exit*,uint64_t,uint64_t,struct lpfg_state*,struct lve_pf_entry*);
#endif
