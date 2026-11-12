#ifndef SYLE_LINUX_DEBUG_DISABLED_H
#define SYLE_LINUX_DEBUG_DISABLED_H
#include "linux_guest_session.h"
/* Only disabled-breakpoint register initialization/readback, NOT active debug. */
struct ldd_state {uint64_t address[4];bool failed;};
bool ldd_apply(struct ldd_state*,struct lgs_session*,uint64_t rip,uint64_t qualification,unsigned length);
#endif
