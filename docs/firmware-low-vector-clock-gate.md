# Next firmware boundary: actual initial count FFFFFFFF

Native evidence after idle-LVT admission: write to FEE00380 with ESI=FFFFFFFF, LVT=20005, initial=0, armed=false. Tiny regression passes. This is the first request that starts a real countdown. The existing observer correctly stops before mutation.

## Decision

Do not connect a running timer until host-clock admission and an actual bounded wakeup mechanism have been verified in the same outer-QEMU environment. CPUID 15/16/80000007 were all zero there; the physical host frequency must not be substituted. Waiting for that runtime proof is a correctness prerequisite, not a reason to substitute fixed timestamps or infer safety from IF=0 or the large count.

A minimal diagnostic profile may support a genuine countdown but stop *before consuming* every unmasked expiry, including vector 5. This does not need to invent ESR/illegal-vector delivery: it is an explicitly incomplete device that terminates the observation when that semantic boundary is reached. No masked transition can erase an already due unmasked event. It may later observe the firmware masking the timer before expiry, but cannot assume that outcome in advance.

## Required transaction

- Admit a verified source, stable epoch/CPU identity, maximum gap, and an explicit rational conversion to virtual APIC ticks. The APIC tick rate is a deliberate device contract, not the physical host APIC rate. Existing lapic_timer uses 1024 source ticks per undivided tick; that unit cannot silently be swapped with PIT ticks, nanoseconds or VMX ticks.
- Read a fresh sample and stage initial count, divisor, absolute deadline, periodic phase and lifecycle state. Reject arithmetic overflow or unsupported clock facts before committing the guest instruction.
- Arrange a VM-exit/wakeup that is no later than the next required observation boundary. VMX preemption counts are a distinct clock domain; the conversion/guard must be established experimentally or use a conservative independently proven bound. An incidental exit loop is not a scheduler.
- Before every entry and register transaction sample time and account expiry. Masked expiry advances phase without replay. Unmasked due retains the timer-owned event and persistently stops. A source error, migration/epoch change, reversal or maximum-gap violation also persistently stops.
- Only after all hardware writes succeed publish staged initial-count state and guest RIP. Partial hardware commit is fatal. The persistent stop/ownership latch must never be rolled back with the speculative device snapshot.

Legal-vector IRQ integration is separately demonstrated by host tests. It does not authorize delivery of vector 5. In the minimal stop-at-expiry profile, IF, interruptibility and APIC priority do not suppress event generation or allow expiry to be discarded.

## Current test scope

`firmware_low_vector_deadline.sh` tests the real FFFFFFFF count in a synthetic clock fixture and an explicitly hypothetical unmasked low-vector running state. It proves the existing session gate stops at the boundary without consuming the timer and stays stopped. Direct fixture mutation deliberately bypasses current initial-count admission and is not a production API. Separate checks exercise verified-source rejection, changed epoch/CPU, reversed time and excessive gap in the existing host-clock model. No runtime frequency, scheduling bound, actual timer start or OVMF progress is claimed.
