#ifndef PIT0_TRANSACTION_H
#define PIT0_TRANSACTION_H
#include "pit0.h"
#include "pic0.h"
struct pt0 {struct pit0 timer;struct pic0 pic;bool failed;};
bool pt0_init(struct pt0*,uint64_t domain);
bool pt0_tick(struct pt0*,uint64_t tick);
bool pt0_io(struct pt0*,uint64_t g[15],uint64_t rip,unsigned cpl,uint32_t reason,uint64_t qualification,unsigned length,uint64_t tick,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
