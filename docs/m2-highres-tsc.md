# Experimental high-resolution guest TSC discovery

## Why another profile

The four Hv reference-clock runs reproduced the slice sensitivity: sleep1 host
receipt intervals for 24/20/20/24 were 1.466405165/1.002341735/0.999705731/
1.439637127 seconds. Final accepted IRQ0/PIT edge ratios were about 71.88%,
99.84%, 99.86%, 70.70%. These whole-session figures support a coalescing/latency
explanation; they are not per-command handler counts or accuracy bounds.
All boots still selected refined-jiffies after PIT-based TSC calibration failed.
Smaller slices improve delivery but do not remove that clocksource's granularity.

## Linux 6.12 mechanism, checked against upstream

Source: https://github.com/torvalds/linux/blob/v6.12/arch/x86/kernel/tsc.c
(`native_calibrate_tsc`). Intel-vendor guests with max basic leaf >= 0x15 can
report the crystal rate and numerator/denominator in CPUID.15. Linux first
converts ECX Hz to integer kHz and computes the ratio with unsigned-int operands,
then returns unsigned long. A nonzero direct crystal rate is treated as a known
frequency. No CPU-model frequency guess is needed for this path. The existing
`tsc_early_khz` boot parameter is an alternative, but this experiment leaves the
command line untouched and does not set `tsc=reliable`/`tsc=nowatchdog`.

## Exact virtual frequency contract

The admitted host-clock profile's existing RDTSC emulator returns
`floor(active_reference_time * 1193182) * 1000`. Its defined virtual TSC rate is
therefore 1,193,182,000Hz, with PIT-domain quantization below one PIT tick
(~838ns). This is not the physical host TSC frequency. The additional CPU view
publishes leaf15 EAX=1000, EBX=1193182, ECX=1000000, EDX=0. Linux's intermediate
product is 1000*1193182=1193182000 (<2^32), yielding exactly 1193182kHz after
division. No truncation or 32/64-bit overflow is hidden. Leaf0 max becomes 15;
unsupported intervening leaves stay zero and all prior baseline features stay
unchanged. RDTSCP and TSC-deadline/APIC features remain unadvertised.

This opt-in view also advertises CPUID.80000007 EDX invariant-TSC bit8 because
within one admitted single-vCPU execution epoch the source rate is fixed across
guest busy execution and HLT, and no supported guest P/C-state operation changes
that rate. This bit is NOT a promise about a physical host TSC, guaranteed wall
accuracy, arbitrary outer-host suspend/live migration, or SMP synchronization.
Explicit management STOP freezes both TSC and device clocks; START re-anchors
without a backwards value or rate change, excluding time when the VM was
paused. CREATE establishes a new machine epoch. Unknown source changes,
backwards samples, or gaps outside the admitted policy fail-stop rather than
altering the published rate. Existing source lifecycle limitations still apply.

## Isolation and verification

`HC_TSC_FREQ=0` retains the prior Hv CPU view and binaries. `HC_TSC_FREQ=1`
selects distinct `hc20-tsc-*` objects and `krnl-managed-host-clock-20-tsc.elf`:

```
make -C efi -f Makefile -f managed-host-clock.mk managed-host-clock \
 HC_BITS=20 HC_TSC_FREQ=1 MB_INPUTS=/absolute/v3-inputs \
 MB_MANIFEST=/absolute/v3-inputs/manifest.json
```

The source must still pass explicit Hv admission before the new guest view is
available. Default/direct and old Hv profiles are unchanged. Host tests verify
exact CPUID arithmetic, unchanged older leaves, invalid-emulation rejection,
failed writes without GPR publication, RDTSC scale at one/two seconds and
unchanged value across explicit STOP/START. The new ELF builds separately.
Native acceptance still requires checking the actual boot clocksource, no
unexpected unsupported MSR/instruction, five-mode observations and cleanup,
then longer-window and pause tests. Publishing frequency is not proof that
Linux selected TSC or that every timing requirement now passes.

## Native observation after publishing frequency

Two isolated runs in evidence Library
`libfile_fde3_f-dbb5d0dc71388191bb02c3434c6a5d99_ab3ac89c6f9c81918c711418a26b62e9`
reported 1193.182MHz and selected `tsc-early`, then `tsc`. Both completed all
five modes and cleanup. Sleep1 host receipt intervals were 1.004400550s and
1.004133770s; guest monotonic deltas were 1.001470041s and 1.001062727s. Spin
monotonic measurements were 1.356038ms and 0.940343ms instead of jiffies' zero.
These are two short samples with different host/guest observation boundaries,
not a long-term drift or hard-real-time guarantee. Same-guest pause evidence
is documented separately in m2-clock-pause.md. Original kernel configuration
is still unknown, so these successful binaries alone are not a reproducible
kernel-build claim.
