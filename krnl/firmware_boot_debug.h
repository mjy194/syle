#ifndef SYLE_FIRMWARE_BOOT_DEBUG_H
#define SYLE_FIRMWARE_BOOT_DEBUG_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Guest-owned software bank. Caller authenticates CPL0, mode, intercepted MOV
 * DR, VMCS identity, DEBUGCTL=0, DR7=400, #DB interception and no event flow.
 * Hardware debug registers are never touched. Active breakpoints are rejected.
 * Any partial callback failure poisons the bank and forbids another entry. */
struct fbd_bank {uint64_t address[4];bool failed,busy;};
bool fbd_apply(struct fbd_bank*,uint64_t regs[16],uint64_t rip,uint64_t flags,
 uint64_t qualification,unsigned length,const uint8_t*,size_t,void*,
 bool(*write)(void*,uint32_t,uint64_t));
/* Hardware-reported MOV DR exit with VM-exit instruction length/qualification. */
bool fbd_apply_qualified(struct fbd_bank*,uint64_t regs[16],uint64_t rip,uint64_t flags,
 uint64_t qualification,unsigned length,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
