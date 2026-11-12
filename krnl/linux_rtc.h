#ifndef SYLE_LINUX_RTC_H
#define SYLE_LINUX_RTC_H
#include <stdint.h>
#include <stdbool.h>
struct lrtc_state {uint64_t tick;uint8_t index,status_c;bool nmi_mask,failed;};
void lrtc_init(struct lrtc_state*);
bool lrtc_io(struct lrtc_state*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t qualification,unsigned length,uint64_t tick,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
