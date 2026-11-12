#ifndef SYLE_LINUX_HIGH_PF_H
#define SYLE_LINUX_HIGH_PF_H
#include "linux_pf_guard.h"
enum lhp_state {LHP_UNUSED,LHP_PENDING,LHP_CONSUMED};
enum {H_REASON,H_INFO,H_ERROR,H_ADDR,H_RIP,H_CR3,H_EVENT,H_VECTOR,H_ACTIVITY,H_BLOCKING,H_CR0,H_CR4,H_EFER,H_RSI,H_RDI,H_RCX,H_RSP,H_CS,H_CSAR,H_SS,H_SSAR,H_GDTR,H_GDTLIMIT,H_CSDESC,H_IDTR,H_IDTLIMIT,H_GATELO,H_GATEHI,H_ROOTENTRY,H_BYTES,H_COUNT};
struct lhp_snapshot {uint64_t v[H_COUNT];bool permissions;};
bool lhp_accept(enum lhp_state,const struct lpfg_state*,const struct lhp_snapshot*);
bool lhp_consume(enum lhp_state,uint64_t reason,uint64_t vector,uint64_t event,enum lhp_state*);
bool lhp_commit(enum lhp_state*,uint64_t*cr2,uint32_t*resume,uint64_t fault,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
