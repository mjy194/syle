#ifndef LINUX_USER_PF_H
#define LINUX_USER_PF_H
#include "linux_nonpresent_pf.h"
/* CPL3 nonpresent instruction-fetch(error14) or write(error6) only, 4-level long mode, IST0 -> CPL0.
 * Same lnp state/history budget as supervisor demand faults. No table repair. */
struct lup_snapshot {
 uint64_t reason,info,error,address,rip,rsp,cr0,cr3,cr4,efer;
 uint64_t cs,csbase,cslimit,csar,ss,ssbase,sslimit,ssar;
 uint64_t idtr,idtlimit,gdtr,gdtlimit,tr,trbase,trlimit,trar;
 uint64_t flags,debug,event,entryerror,entrylen,vector,blocking,activity;
};
typedef bool (*lup_read)(void*,uint64_t,void*,size_t);
bool lup_validate(const struct lnp_state*,const struct lup_snapshot*,uint64_t ram,void*,lup_read);
bool lup_inject(struct lnp_state*,struct lgs_session*);
#endif
