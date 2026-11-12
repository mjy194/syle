#ifndef SYLE_LINUX_PIC_MASK_H
#define SYLE_LINUX_PIC_MASK_H
#include <stdint.h>
#include <stdbool.h>
struct lpic_chip {uint8_t phase,vector,cascade;bool started,aeoi;};
struct lpic_state {uint8_t master,slave;bool failed;struct lpic_chip chip[2];};
bool lpic_entry_ok(const struct lpic_state*,uint64_t rflags);
bool lpic_io_flags(struct lpic_state*,uint64_t gpr[15],uint64_t rip,unsigned cpl,
 uint64_t reason,uint64_t qualification,unsigned length,uint64_t rflags,void*,bool(*write)(void*,uint32_t,uint64_t));
void lpic_init(struct lpic_state*);
bool lpic_io(struct lpic_state*,uint64_t gpr[15],uint64_t rip,unsigned cpl,
 uint64_t reason,uint64_t qualification,unsigned length,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
