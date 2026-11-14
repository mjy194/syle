# Hv reference clock: entry-latency A/B follow-up

The first native Hv run passed source admission, all five probe modes and
checked cleanup. The negative `-cpu host` run rejected admission before Linux.
This is not a wall-clock accuracy acceptance.

Its boot log explicitly says `Fast TSC calibration failed`, `Unable to calibrate
against PIT`, `No reference (HPET/PMTIMER) available`, and switches to
`refined-jiffies`. CLOCK_MONOTONIC/RAW are therefore tick-driven, roughly 4ms
resolution here. Spin returned zero for both, while intercepted guest RDTSC
advanced by 1,038,000 units, about 0.870ms at the defined virtual 1,193,182,000Hz.
Thus zero is not evidence that the new reference clock stopped advancing.

For sleep1, RDTSC advanced by 1,673,412,000 units = 1.40248s, host BEGIN-to-END
receipt was 1.40875s, but Linux monotonic advanced only 1.0000625s. These
nonidentical sampling boundaries cannot certify accuracy, but the result
supports testing interrupt-delivery lag/coalescing rather than multiplying
clock conversion by an empirical constant. PIT/PIC edges are not one-to-one
with delivered or serviced guest interrupts when pending edges coalesce.

The new make target accepts `HC_BITS=24` or `HC_BITS=20`. All hc objects and
kernel filenames contain the bits value, preventing stale-object cross-use:

```
make -C efi -f Makefile -f managed-host-clock.mk managed-host-clock HC_BITS=24 \
 MB_INPUTS=/absolute/clock-v3-inputs MB_MANIFEST=/absolute/clock-v3-inputs/manifest.json
# Repeat with HC_BITS=20.
```

Pair `krnl-managed-host-clock-24.elf` and `-20.elf` separately with the same
terminal manager, same v3 inputs, QEMU11.1.3 or recorded actual version,
`-cpu host,hv-time=on`, single vCPU and independent fresh ESP/VARS. Run serially
24/20/20/24 with the existing five-mode observer, one repeat per fresh VM.
Require source log, exact probe markers and checked cleanup. Retain full boot
logs to identify the actual selected Linux clocksource in each run.

Diagnostics are equal in both arms and emit only at STOP/release:
- `SYLE.CLOCK_DIAG`: whole-session accepted exits, preemption exits and summed
  nonserialized entry TSC windows (includes post-exit host validation).
- `HOST_CLOCK.STOP`: committed virtual ticks, total PIT edges, IRQ0 accepted
  injection receipts, and min/max gap in virtual ticks between entry snapshots
  for those accepted IRQ0 injections. This does NOT timestamp guest IRQ handler
  execution, does not prove Linux processed every injection, and is not a
  per-command count. An accepted receipt is counted only after successful
  ownership reconciliation, when its completed serial changes.

Changing entry bounds is a latency experiment, not clock-source calibration
and not a hard deadline guarantee. No guessed physical TSC/APIC frequency is
introduced. The default production profile remains unchanged.
Repeated STOP/cleanup can print the same cumulative session counters more than
once. Select the final snapshot; never sum duplicate snapshots.
