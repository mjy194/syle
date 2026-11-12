#ifndef SYLE_LINUX_INVLPG_H
#define SYLE_LINUX_INVLPG_H
#include "linux_guest_session.h"
/* No-VPID, 4-level CPL0 contract; false forbids further entry. */
bool linv_apply(struct lgs_session*,uint64_t rip,uint64_t linear,unsigned length);
#endif
