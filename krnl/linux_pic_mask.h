#ifndef SYLE_LINUX_PIC_MASK_H
#define SYLE_LINUX_PIC_MASK_H
#include <stdint.h>
#include <stdbool.h>
struct lpic_state {uint8_t master,slave;bool failed;};
void lpic_init(struct lpic_state*);
bool lpic_io(struct lpic_state*,uint64_t gpr[15],uint64_t rip,unsigned cpl,
 uint64_t reason,uint64_t qualification,unsigned length,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
