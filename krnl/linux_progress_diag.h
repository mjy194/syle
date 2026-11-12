#ifndef SYLE_PROGRESS_DIAG_H
#define SYLE_PROGRESS_DIAG_H
#include "linux_guest_dispatch.h"
#include "linux_guest_session.h"
struct lprog_snapshot {uint64_t exits,ticks,reload,count,edges,irr,isr,reserved,phase,reason,rip,stage,result;bool timer_valid,vmcs_valid;};
struct lprog_gate {uint64_t samples,expected,records;unsigned seen;bool awaiting;};
/* Pure bounded sampling schedule. Before uses next completed exit number. */
bool lprog_due(struct lprog_gate*,uint64_t exits,bool after,bool success);
bool lprog_mark(struct lprog_gate*,unsigned milestones);
bool lprog_collect(const struct lgd_context*,struct lgs_session*,struct lprog_snapshot*);
#endif
