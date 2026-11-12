#ifndef SYLE_LINUX_GUEST_SNAPSHOT_H
#define SYLE_LINUX_GUEST_SNAPSHOT_H
#include "linux_high_pf.h"
struct lgs_session;
/* Read-only fixed-image high-PF evidence. Reads exclusively owned private RAM
 * through memory.owner.map. Never repairs mappings or writes guest tables. */
bool lgds_high(struct lgs_session*,uint64_t reason,uint64_t rip,uint64_t address,
 uint64_t event,uint64_t vector,struct lhp_snapshot*);
#endif
