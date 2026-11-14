#ifndef SYLE_LINUX_PF_DIAGNOSTIC_H
#define SYLE_LINUX_PF_DIAGNOSTIC_H
#include "linux_pt_diag.h"
struct lgs_session;
#define LGPD_FIELDS 34u
#define LGPD_TRACES 12u
struct lgpd_snapshot {uint64_t value[LGPD_FIELDS],valid,r12,va[LGPD_TRACES],gate[2],csdesc,bytes[2],handler_bytes[2];uint64_t target_csdesc,trdesc[2],tss_rsp0,selected_stack,selected_slot;unsigned data_valid;struct lpt_trace trace[LGPD_TRACES];};
/* Pure bounded read-only observation. Resolved traces do NOT prove permissions.
 * Field order: reason,event,error,qual,rip,cr0,cr3,cr4,efer,idtr,idtlimit,gdtr,
 * gdtlimit,rsp,cs,csar, csbase,cslimit,ss,ssbase,sslimit,ssar,
 * tr,trbase,trlimit,trar,rflags,pendingdebug,entry_info,entry_error,entry_length,vectoring,
 * interruptibility,activity. Traces: fault,rip,idt14,gdtcs,rsp-32,handler, targetcs,trdesc,tss_rsp0,selected_slot,stack_frame_first,stack_frame_last.
 * data_valid bits0..3 existing; bit4 targetCS, bit5 TRdesc, bit6 RSP0,
 * bit7 selected stack. Traces alone NEVER prove access permission. */
bool lgpd_collect(struct lgs_session*,struct lgpd_snapshot*);
#endif
