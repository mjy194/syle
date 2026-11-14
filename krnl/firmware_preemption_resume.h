#ifndef SYLE_FIRMWARE_PREEMPTION_RESUME_H
#define SYLE_FIRMWARE_PREEMPTION_RESUME_H
#include <stdint.h>
#include <stdbool.h>
/* Pure admission only. VMX preemption exit did not emulate an instruction:
 * caller MUST preserve RIP/RFLAGS (including RF) and all guest state. */
bool fpr_resume(uint64_t reason,uint64_t cs,uint64_t flags,uint64_t blocking,uint64_t pending_debug,uint64_t dr7,uint64_t entry_event,uint64_t vectoring);
#endif
