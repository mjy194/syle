#ifndef SYLE_LINUX_PORT80_H
#define SYLE_LINUX_PORT80_H
#include <stdbool.h>
#include <stdint.h>
struct lp80_state {uint64_t writes;uint8_t last;bool failed;};
/* Private guest POST latch, not host I/O or calibrated delay. */
bool lp80_io(struct lp80_state*,const uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t qualification,unsigned length,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
