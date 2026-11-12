#ifndef SYLE_NESTED_KICK_H
#define SYLE_NESTED_KICK_H
#include <stdbool.h>
#include <stdint.h>
/* Local CPU only, IF=0. Caller supplies its identity-mapped UC xAPIC page,
 * previously established by paging_smp_prepare. No mapping or IDT changes.
 * Restricted modern xAPIC (max LVT 5/6), not x2APIC or a guest virtual APIC.
 * Root vector 0x41 stays the default fail-closed sink; VMX must acknowledge
 * external-interrupt exits so consume sees a real in-service interrupt. */
bool nested_kick_init(uint64_t lapic_base,unsigned expected_apicid);
/* Requires exact external-interrupt exit information 0x80000041 and no other
 * in-service vectors. Unmatched/precondition failure never issues EOI.
 * After EOI failure to observe ISR clear returns false: caller retains/stops,
 * and must not retry blindly. Does not reenable interrupts. */
bool nested_kick_consume(uint64_t lapic_base,uint32_t exitinfo);
#endif
