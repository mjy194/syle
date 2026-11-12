#ifndef SYLE_LINUX_PF_DIAGNOSTIC_H
#define SYLE_LINUX_PF_DIAGNOSTIC_H
#include "linux_pt_diag.h"
struct lgs_session;
#define LGPD_FIELDS 16u
#define LGPD_TRACES 6u
struct lgpd_snapshot {uint64_t value[LGPD_FIELDS],valid,r12,va[LGPD_TRACES],gate[2],csdesc,bytes[2],handler_bytes[2];unsigned data_valid;struct lpt_trace trace[LGPD_TRACES];};
/* Pure bounded read-only observation. Resolved traces do NOT prove permissions.
 * Field order: reason,event,error,qual,rip,cr0,cr3,cr4,efer,idtr,idtlimit,gdtr,
 * gdtlimit,rsp,cs,csar. Traces: fault,rip,idt14,gdtcs,rsp-32,handler. */
bool lgpd_collect(struct lgs_session*,struct lgpd_snapshot*);
#endif
