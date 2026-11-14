# Next-exit bound: available evidence and missing guarantee

100 MHz is the chosen virtual APIC platform frequency, not a discovered hardware frequency. Hv#1 10 MHz reference time is a different domain from VMX preemption ticks. Measurements of observed ratios and percentile latencies cannot turn these domains into a deterministic next-exit wall-time bound.

In nested virtualization the outer vCPU may be descheduled or suspended while Hv reference time advances. A VMX execution budget need not count that pause. Therefore even exact TSC frequency knowledge does not by itself give a finite upper bound on the next reference-time sample. The current API cannot truthfully issue a deadline certificate for this environment. Native running-timer admission remains disabled.

## What can be tested now

Use a synthetic bound in host tests only. Record epoch, before-entry reference sample, agreed latest sample, timer generation and reservation generation. Reject entry if there is no trusted bound, it predates the current sample, it exceeds max-gap, it reaches/crosses the exact APIC expiry, or another transaction owns the domain. Consume this record exactly once on the next known exit. Epoch change, contradictory entry outcome, sample reversal, observed pause, bound overrun or missing receipt permanently stops the domain. Never retry with a new sample to erase that failure. Timer IRQ ownership stays in the unconsumed device or reserved IRR until teardown.

This receipt validation detects violated assumptions after exit. It does NOT establish that a guest cannot execute past a deadline before exit. Label it observation/fail-stop, never reliable wakeup.

## Acceptable future sources of a bound

- An outer runtime with an explicit, enforceable deadline/wakeup and scheduling contract, including suspend semantics and worst-case latency, observable to this layer.
- A deliberately different virtual-device clock that advances only with guest execution, backed by a real execution-time accounting mechanism and consistent firmware reference/calibration sources. Wall/reference time cannot silently be relabelled as execution time.

A fixed preemption count with repeated reference-time checks can be a useful best-effort diagnostic experiment, but changing to that contract requires explicit scope approval and clear reporting. A measured ratio, generous guard margin, IF=0, large FFFFFFFF count, or a successful short run is not a deterministic timing proof.

For the strict current profile: no bound source means no timer start. The existing stopped-at-initial-count result remains the last native milestone. Host clock-source experiments and the standalone arithmetic model advance prerequisites, not OVMF boot completion.

## External-interrupt candidate: evidence plan, not implementation approval

An outer virtual LAPIC timer plus VMX external-interrupt exiting may be able to force an exit before a resumed L2 executes after an expired outer deadline. This is materially different from merely observing time on a later incidental exit. It still does not establish a host wall-clock scheduling bound: no interrupt forces a descheduled process to run by a deadline.

Before implementing a firmware backend, establish the architectural and QEMU/KVM nested event ordering for pending outer IRQ versus nested entry, external-interrupt exiting, interrupt acknowledgement, host IF/blocking, virtual interrupt delivery/APICv, and suspend/resume. Ownership of the outer timer/interrupt vector and EOI must be exclusive; it cannot hijack Linux's diagnostic IRQs or silently consume a host interrupt. No OS security/network change is needed or authorized by this design.

A separate minimal test target could provide falsifying evidence:

1. L2's first operation stores a marker; normal continuation repeats bounded work and records progress. The observer checks whether a pending outer timer produces an external-interrupt exit before the first marker/next continuation.
2. Exercise deadlines already due before entry, due during active L2 execution, and due while the outer vCPU is paused. Vary host interruptibility and supported APIC virtualization configurations explicitly.
3. On resumption capture exit reason, vector/ack state, marker, reference sample, timer programming and EOI receipt. If progress occurs after an expired wakeup that was required to precede L2, reject that backend.
4. Test programming failure, no available vector, stale/duplicate IRQ, masked source, spurious IRQ, rearm race and cleanup failure. Missing/ambiguous receipt retains ownership and stops.
5. Compare source-domain timestamps, not guest refined-jiffies. Never count a timeout or missing log as a successful interrupt exit.

Successful finite tests support only the documented tested configuration. A safety guarantee also requires a source/implementation argument for event precedence and timer persistence across scheduling pauses. If those cannot be established, the result remains best-effort wakeup with explicit limitations, unsuitable for the strict running-timer profile.
