#ifndef SYLE_FIRMWARE_PAUSE_H
#define SYLE_FIRMWARE_PAUSE_H
#include "firmware_apic_transaction.h"

struct fpa_state { uint64_t completed; bool busy,failed; };
struct fpa_trace {
 uint64_t rip,flags,length,primary,secondary;
 unsigned available;
 uint8_t bytes[2];
};
/* Fresh zeroed state for one serialized VMCS lifetime. The caller excludes
 * guest execution and freezes owned memory/page tables throughout this call.
 * The outer adapter proves current VMCS/EPT identity and safe host recovery.
 * Reads actual VMCS state and owned F3 90 bytes; supports only the existing
 * CPL0 long-mode firmware profile with PAUSE exiting and no PLE.
 * Retires RF then RIP, publishing completed last. A failed VMWRITE may have
 * taken effect: latch failed permanently, never resume/retry. Every rejection
 * also requires the caller to stop. No GPR, memory, timer or control writes.
 * VM-exit qualification has no PAUSE operand meaning and is not consulted. */
enum fat_result fpa_handle(struct fpa_state*,struct firmware_memory*,bool exclusive,
 void*,bool(*read)(void*,uint32_t,uint64_t*),
 bool(*write)(void*,uint32_t,uint64_t),struct fpa_trace*);
#endif
