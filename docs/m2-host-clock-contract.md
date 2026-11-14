# Opt-in host-clock contract (experimental wiring; not runtime-validated)

## Evidence and goal

The four isolated runs in order 28/24/24/28 returned guest nanosleep(1s)
receipt intervals 22.281032292/1.554598798/1.544906604/22.285369836 host seconds.
All reported approximately 1.000067878 guest seconds and checked cleanup.
Each used the same guest inputs, management ELF and observer. Reversing order
reproduced the difference. This establishes entry-budget sensitivity, not a
calibrated clock or exact assignment of all delay to preemption.

The opt-in design makes logical elapsed time independent of exit count and
slice length. Slice length still bounds interrupt-delivery latency. The legacy
exit-driven profile and accepted baseline must remain available unchanged.

## One clock domain

`linux_host_clock` converts a verified host counter into 1,193,182 ticks/sec,
the existing PIT domain. PIT0, PIT2, PIC timer progression, UART RX timeout and
intercepted guest RDTSC (tick * 1000) must all consume **the same staged tick**.
No second independent increment, `max(old+1, sample)`, or idle deadline jump is
allowed. Multiple exits may legitimately observe the same tick. Existing APIC
`now` validation assumes `ticks+1` and must explicitly distinguish this new
profile before integration; do not relax that invariant for the legacy path.

Take one host sample per dispatch; stage clock state and all device changes,
then commit them together only after successful emulation. Any failure latches
no-resume. A no-mutation failure return from the planner is not permission to
retry a suspicious counter or silently switch back to exit-driven time.

## Source admission, frequency and migration

No CPU model-name MHz, guessed constant or frequency derived from guest timing.
Native integration must supply a checked invariant counter frequency (exact
CPUID ratio with a valid crystal rate, or an independently calibrated platform
reference with documented uncertainty), and record provenance. The planner
currently admits only an explicitly verified integer Hz in [1MHz, 10GHz]. A
nonintegral ratio is unsupported until its exact rational conversion exists.
Do not truncate that ratio silently. CPUID.16 nominal MHz is not sufficient.

The single-BSP vCPU is pinned to a stable source CPU and generation/epoch.
Every sample must match both. Backwards TSC, changed CPU/epoch or an elapsed
gap beyond the configured maximum fails closed. Invariant TSC alone does not
prove cross-core offsets or migration continuity. A host VM may be migrated
without changing guest-visible CPU ID; its provider must guarantee continuity
or an externally observable epoch, otherwise this profile cannot claim safety
under that migration. Suspend/resume requires a new admission/re-anchor policy;
it is not silently included as elapsed guest runtime.

## Running, halting and pausing

While RUNNING, elapsed host time includes guest execution, guest HLT, VM exits,
manager/console handling and time waiting for the next STEP. HLT must not freeze
time and must not jump to a future PIT edge. Preemption/host timer wakeups bound
when due interrupts become visible; a shorter bound is a latency policy, not a
clock-rate change. Future adaptive entry deadlines require their own bounds.

Explicit STOP samples and commits the final running interval, then freezes
virtual tick plus fractional remainder. START re-anchors to a valid newer host
sample without charging the paused duration. Repeated STEP is not START: do not
re-anchor on every entry. CREATE resets the epoch's guest origin. DESTROY ends
it. Integrating these lifecycle operations requires coordination with the
management service, not merely changing `lgs_session.start`, which is invoked
for individual STEP execution today.

## Numerical guarantees and limits

Integer seconds/remainder decomposition avoids overflowing delta * tick_rate.
Fractional remainder is carried across every sample and STOP/START, so sample
frequency causes no accumulated rounding loss. Tick is nondecreasing, not
strictly increasing, and <= UINT64_MAX/1000 so guest RDTSC cannot overflow.
No modular counter-wrap interpretation is supported. Elapsed source gaps are
bounded by an explicit admission policy, never silently clamped. Planner tests
cover many-small vs one-large samples, hour advancement, freeze/re-anchor,
invalid sources, CPU/epoch change, backwards samples, gap and tick overflow.

## Before enabling a native target

1. Inspect and record actual host frequency/reference capabilities.
2. Add an explicitly selected dispatcher profile, stage/commit the planner,
   disable legacy idle advancement only there, and wire STOP/START separately
   from STEP. Preserve default profile validation and tests.
3. Exercise PIT/UART common-domain behavior, missed/coalesced PIT edges, pending
   IRQ ownership, paused events, fail-stop and cleanup under large elapsed gaps.
4. Repeat 0/10/100/1000ms/spin with different entry bounds, then 10s/60s elapsed
   tests against host monotonic time; report rate error and delivery latency
   separately. Repeat explicit STOP/START and CPU/source rejection tests.
5. Accept no new wall-clock claim before those native measurements. The pure
   conversion module passing host tests is not a Linux runtime result.

### External I/O ordering prerequisite

Synchronizing only on VM exit is insufficient. Before a running external
CONSOLE_SEND, advance the common domain to the input's arrival time, then
receive/reset its age. Otherwise elapsed time before arrival is incorrectly
charged to the new byte and may cause an immediate false UART timeout. STOP
must commit the final common device time with clock state, not freeze only the
conversion planner. The existing PIC04 domain refuses advancement while a
reserved interrupt ticket exists. Integration must use an ownership-safe point
or explicit deferred event timestamp, never bypass that reservation check.

## Experimental Hyper-V reference-counter wiring

The physical archbook CPUID.15 gives 3.072 GHz and invariant TSC, but **inside
its actual QEMU `-cpu host` all .15/.16/.80000007 values were zero**. That
source is rejected; using the physical rate in that guest is not permitted.

A separate `managed-host-clock.mk` target now wires a Hyper-V reference-counter
source into an isolated `hc-*` kernel. It requires the hypervisor bit, Hv#1
interface, sufficient feature leaves and the advertised reference-count MSR
feature. Reads of 0x40000020 reuse the existing armed native #GP/#UD fixup and
are read-only. Missing/contradictory advertisement, inaccessible MSR, backward
sample, changed virtual CPU or an active gap over 60 seconds is a failure.
Two admission samples may be equal because the counter resolution is 100ns;
the value's origin may legally be zero. The fixed 10MHz unit comes from this
specific Hyper-V ABI, **not** a claimed physical TSC frequency. `hv-frequencies`
is not required and is not treated as independent calibration evidence.

This profile is not enabled on physical hosts and is not an automatic fallback.
QEMU would need an explicit separately reviewed `hv-time` CPU contract for a
future run. An environment without it must reject admission. Even successful
admission is only a feature/consistency check, not proof of host-wall-time
accuracy. Validate against the observer's monotonic clock in multiple windows,
under busy/HLT, STOP/START and different entry bounds. Host suspend/live migration
remain outside this experiment's supported lifecycle; same virtual CPU ID alone
does not detect them. The profile uses a 2^24 entry bound to limit delivery
latency, not to define clock rate. No native timing result exists yet.

`clock_start` only re-anchors a stopped clock; successive STEP calls retain its
anchor. The before-entry safe point advances time before IRQ reservation, and
dispatch stages a second sample with emulation and commits both only on success.
External SEND first checks the reservation: busy means no byte accepted and no
sample consumed, so a later normal STEP can release the ownership before retry.
It then synchronizes elapsed time before pushing the new byte. STOP advances
and freezes the clock and device domain together. An unsafe STOP synchronization
fails and latches no-resume; a later teardown stop is allowed to quiesce/clean
already-failed state, never to restart it. A partial guest VMCS write is not
rolled back by this layer; failed dispatch remains terminal as before.

Host tests cover exact conversion, source/fault admission, the existing PIC04
regression, shared-domain timeout, repeated STEP/same-tick exits, paused time,
RX ordering, busy reservation, failed source, failed reads/writes without clock
commit and cleanup-after-failure. These are not live KVM acceptance tests.

For physical machines lacking CPUID.15 or invariant TSC, a future alternative is
an ACPI-described PM counter or HPET reference with checked I/O/MMIO ownership,
wrap handling and multiple calibration windows. A single guessed PIT delay,
CPU nominal MHz or one startup calibration cannot establish long-term frequency
stability. No such fallback is implemented or silently selected here.

Before-entry clock synchronization commits independently before later VMCS
preparation. A subsequent preparation failure is terminal/no-resume; it does
not roll back that already-observed passage of time. Dispatch emulation has
its separate staged clock/device commit boundary.
