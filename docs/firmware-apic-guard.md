# Active xAPIC register-only guard (not wired to a VM)

`firmware_apic_guard` is an independent pure wrapper around `guest_apic`.
It does not change the running/frozen disabled-APIC observer candidate.
Before future wiring, a distinct active CPU profile must advertise APIC=1,
x2APIC=0 and route APIC_BASE plus the fixed MMIO window consistently.

The initialized virtual controller is fixed xAPIC/BSP atFEE00000. APIC_BASE
reads FEE00900 and accepts only an identical write. Disable, relocation and
x2 mode changes are rejected. Internally guest_apic uses an x2-encoded priority
core even for xAPIC mode; that internal base is never returned to the guest.

Only no-event register behavior is allowed. ICR low/high, self-IPI, unmasked
LVTs and nonzero timer initial count are rejected before model invocation.
Any staged EOI effect or unexpected pending/in-service state rejects the
transaction. Empty EOI is allowed as a genuine empty-controller no-op. No
source is connected; no interrupt is consumed, suppressed or falsely delivered.

Timer count is zero because the timer cannot be started, not because time is
frozen while pretending it runs. There is no clock input or frequency promise.
The wrapper always passes time0 to the unstarted timer model; once countdown
is added this API must change and be reviewed with a documented time domain.
The existing 1024-base-tick timer does not automatically define physical Hz.

All operations stage the model and output and commit only on full success.
A future VM-exit adapter must additionally stage this wrapper, commit guest
RIP/GPR, then publish the staged model. Partial VMWRITE failures must fail-stop.
This pure wrapper alone does not provide EPT decode, CPU exposure, MMIO address
validation or architectural exception injection.

Tests cover fixed base/mode, ID/version, TPR/SVR, masked timer configuration,
stopped countdown, rejected event generation, malformed widths/offsets,
empty EOI, unexpected pending state and failure output/state preservation.
