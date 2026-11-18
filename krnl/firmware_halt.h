#ifndef SYLE_FIRMWARE_HALT_H
#define SYLE_FIRMWARE_HALT_H
#include "firmware_apic_transaction.h"

struct fhl_state {
 uint64_t completed,idle_exits,wakeups,resume_rip,halt_flags;
 bool halted,busy,failed;
};
struct fhl_trace {uint64_t rip,flags,blocking,activity,reason;unsigned available;uint8_t opcode;};
/* One serialized, zero-initialized VMCS lifetime. Outer adapter verifies
 * current VMCS/EPT identity, host restoration, exclusive owned memory, and
 * the existing clock/IRQ contract. hlt_supported comes from IA32_VMX_MISC[6].
 * Exact CPL0 long-mode F4 only, IF=1, no debug/event state. Retire RF and the
 * STI/MOV-SS shadow, advance RIP once, then publish guest activity HLT.
 * No GPR, memory, timer, interrupt or control changes. Every rejection stops;
 * any uncertain write or reentrant commit latches terminal failure. */
enum fat_result fhl_handle(struct fhl_state*,struct firmware_memory*,bool exclusive,
 bool hlt_supported,void*,bool(*read)(void*,uint32_t,uint64_t*),
 bool(*write)(void*,uint32_t,uint64_t),struct fhl_trace*);
/* Call after an entry that began logically halted, before another entry.
 * proven_exit includes successful entry, host restoration and fresh identity.
 * injected means a real staged vector32 transaction; receipt_verified must
 * come from the existing ftid_after bridge, never from entry-info or RIP.
 * Without injection only timer52 at unchanged RIP/flags is admitted. Hardware
 * may save activity=0 on that exit: restore HLT without retiring another byte.
 * With a verified injection, hardware must be active; release the software
 * wait state, but never write active state or invent/consume an IRQ. */
enum fat_result fhl_after(struct fhl_state*,bool exclusive,bool proven_exit,
 bool injected,bool receipt_verified,void*,bool(*read)(void*,uint32_t,uint64_t*),
 bool(*write)(void*,uint32_t,uint64_t),struct fhl_trace*);
#endif
