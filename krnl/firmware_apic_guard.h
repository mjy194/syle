#ifndef SYLE_FIRMWARE_APIC_GUARD_H
#define SYLE_FIRMWARE_APIC_GUARD_H
#include "guest_apic.h"
/* Independent active-xAPIC guarded register profile.
 * This model has no time source, timer start, IPI, external source or interrupt injection.
 * Fresh zero-initialized object required; construct and mutate only via this
 * API. fag_valid checks source-free guard invariants, not arbitrary corrupted
 * register state. Caller stages this object across RIP/register commit. */
struct fag_state { struct guest_apic apic; bool ready; bool lint0_extint_deasserted; bool lint1_nmi_deasserted; };
/* Opt-in only for a platform with permanently deasserted LINT0 and no
 * producer/router. Caller must recheck quiet PIC/event ownership each entry.
 * Does not enable ExtINT delivery. Default/other profiles leave this false. */
bool fag_lint0_quiet_enable(struct fag_state*);
/* Separately proven no guest NMI producer/router; IF is not an admission gate.
 * Host NMIs must VM-exit and stop, never be forwarded or silently consumed. */
bool fag_lint1_quiet_enable(struct fag_state*);
/* Host asynchronous signals must exit, not enter the quiet guest pins. */
bool fag_host_events_exit(uint64_t pin_controls);
bool fag_init(struct fag_state*);
bool fag_valid(const struct fag_state*);
bool fag_base(struct fag_state*,bool write,uint64_t *value);
bool fag_mmio(struct fag_state*,uint64_t offset,unsigned width,bool write,uint32_t *value);
#endif
