#ifndef SYLE_LINUX_PAUSE_H
#define SYLE_LINUX_PAUSE_H
#include "linux_guest_session.h"
bool lpause_apply(struct lgs_session*,uint64_t rip,unsigned length);
#endif
