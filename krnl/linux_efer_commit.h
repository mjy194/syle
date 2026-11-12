#ifndef SYLE_LINUX_EFER_COMMIT_H
#define SYLE_LINUX_EFER_COMMIT_H
#include <stdbool.h>
#include <stdint.h>
/* Narrow fixed-active-long-mode diagnostic contract, not generic WRMSR:
 * input must retain current LME/LMA bits. A differing LMA input is unsupported
 * and stops, rather than pretending to change the read-only hardware state.
 * Only SCE/NXE may change; long-mode bits are constructed from current EFER.
 * Caller must stop on any failure, including partially committed VMCS. */
struct lefer_env {uint64_t efer,cr0,cr4,entry,eptp;uint32_t ext_edx;};
bool lefer_apply(uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,unsigned len,const struct lefer_env*,void*,bool(*write)(void*,uint32_t,uint64_t),bool(*invept)(void*,uint64_t));
#endif
