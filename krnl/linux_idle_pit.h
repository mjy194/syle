#ifndef SYLE_IDLE_PIT_H
#define SYLE_IDLE_PIT_H
#include "pit0_transaction.h"
struct lip_idle {uint64_t reason,activity,flags,blocking,event,error,length,vector;bool owner;};
/* Return checked next logical time; no writes, output unchanged on error. */
bool lip_plan(const struct pt0*,uint64_t tick,const struct lip_idle*,uint64_t*next);
#endif
