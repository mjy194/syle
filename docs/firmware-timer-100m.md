# Nondefault 100 MHz virtual APIC clock prototype

The timer's platform contract is explicitly 100 MHz virtual APIC bus, with architectural dividers 1,2,4,8,16,32,64,128. This is a selected virtual-device frequency, not the physical host's APIC clock. Reference time is an admitted 10 MHz Hv#1 reference counter: one 100 ns increment equals exactly ten virtual bus ticks. No PIT-rate intermediate, TSC ratio, float or `BASE_TICKS=1024` conversion is used.

The module is not linked into any observer. The Hv test profile has real-run source evidence, but tick-delivery accuracy and a deadline wakeup bound remain unproven. Refined-jiffies behavior in the Linux experiment is not proof of APIC timing accuracy. There is no native reader, VMX programmer or claim of timely interrupts in this module.

## Arithmetic and state

A session-relative origin avoids multiplying the full source counter. Every sample checks epoch, monotonicity, maximum observation gap and x10 multiplication overflow. Any violation permanently stops the session. Deadline and period are held in bus ticks, preserving sub-reference-tick phase. Current count uses ceiling division by the selected divider. Masked periodic expirations advance from the old deadline, not from the observation time. An unmasked expiry, including low vector 5, permanently stops before consuming the event. Reinitialization, cancellation or a later mask cannot clear that boundary.

Actual OVMF initial FFFFFFFF, divide2, LVT20005 would have its first expiry at 85.8993459 seconds in this explicitly chosen platform. This is a model consequence, not evidence that guest execution can safely run unchecked that long. IF is irrelevant to source event generation.

`start` observes the existing timer before applying register programming. A bad new configuration can therefore retain a valid elapsed-time observation or masked-expiry accounting; this is intentional event-before-command ordering, not a fully atomic MMIO transaction. A future instruction wrapper must stage device updates but preserve terminal lifecycle state independently if hardware commit fails. Zero initial cancels only when no due unmasked event has already been observed.

## Entry/wakeup boundary

`entry_before` checks an externally proven latest next-exit sample against the exact bus deadline, strictly before expiry, including fractional 100 ns boundaries. The argument is not an untrusted guessed budget. Without an independent proof that the wakeup mechanism supplies that upper bound, no guest entry is permitted. Existing fixed 2^24 VMX budget alone provides no conversion to Hv reference time. Scheduler delay and time spent after guest exit must be part of the bound or force a terminal late-sample stop.

The current pure predicate does not generate a wakeup certificate or program hardware. Native integration must own all state, sample immediately before entry, arrange an actual wakeup, and fail-stop if the observed outcome contradicts its bound. Timer/device state and VMCS/RIP publication must be a separate transaction. Legal-vector IRQ handoff remains a separate option; the current profile stops at every unmasked expiry and implements no illegal-vector ESR delivery.

## Tests

`sh krnl/tests/firmware_timer_100m.sh`: ASan/UBSan and freestanding build; actual FFFFFFFF boundary, last safe sample/current count, exact expiry preservation, terminal reinit/cancel rejection, sub-100ns periodic phase, each divider, epoch and gap failures. This provides arithmetic/lifecycle evidence only, not a native source or wakeup certification.

## Proposed practical M2 observation contract (separate from strict gate)

A non-hard-real-time device can use verified reference time and a bounded guest execution budget between samples without claiming a bound on wall-clock latency. Every exit, entry boundary and timer MMIO first samples elapsed time. Current-count reads use real elapsed reference time. Mask/cancel cannot erase a due event. A future legal-vector extension may transfer atomically to IRR for delayed delivery; that ownership path is demonstrated only by host composition tests. This native observation profile stops on every unmasked expiry and does not inject any IRQ. Low-vector expiry remains an explicit stop with the event retained until teardown, not a fake ESR success.

This weaker contract permits guest instructions to execute after the ideal deadline before a sampling exit; it does not promise exact interrupt ordering at that time. This is an explicit device limitation, not a hard-deadline proof. Scheduler pauses can cause arbitrary wall latency; “bounded” refers only to the programmed guest execution slice, not wall time. Using this profile requires selecting it expressly instead of silently treating `entry_before`'s missing proof as satisfied.

For the observed OVMF sequence, initial count FFFFFFFF may run honestly; if the firmware masks before actual expiry the timer continues as a masked counter. If observation discovers an unmasked expiry first, the run stops. Neither IF=0 nor the nominally long period authorizes ignoring an event. This can advance a normal firmware milestone without pretending to offer hard-real-time virtual interrupts, while preserving fail-stop at still-unimplemented vector-5 semantics.
