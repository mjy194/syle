# Milestone 2 guest interrupt/timer device-model increment

## Implemented

- `lapic_timer_init_guest()` opts into a periodic-capable guest policy. Unlike
  the old diagnostic profiles, the guest may change LVT vector, mask and
  one-shot/periodic mode while counting. This preserves current deadline and
  periodic phase. At a due deadline, poll first so the old vector/mask owns the
  elapsed event; writes otherwise reject atomically.
- `lapic_timer_mmio()` provides aligned 32-bit xAPIC timer register aliases at
  offsets 0x320, 0x380, 0x390 and 0x3e0. State is shared with existing x2APIC MSR
  accessors. The caller is responsible for checking APIC mode and GPA mapping.
- `ioapic_init_guest()` opts into active-low routing and mask-only updates of
  an in-service level line. The saved vector/trigger/destination cannot change
  until EOI. Masking retains Remote IRR; EOI clears it without redelivery while
  masked. Unmask after EOI redelivers an asserted level line.
- `ioapic_set_level()` accepts an electrical level and translates programmed
  polarity. Existing `ioapic_set_irq()` accepts **logical assertion**, unchanged.
  Do not apply polarity twice. Polarity reprogramming requires the device to
  re-drive its current electrical level. Edge history is logical.

Old `lapic_timer_init`, `lapic_timer_init_mode` and `ioapic_init` remain strict
and keep their previous regression tests. New policy does not silently alter
existing diagnostic payload assumptions.

## Integration

Apply changed krnl/lapic_timer.{c,h}, krnl/ioapic.{c,h}; add
krnl/tests/guest_timer.c, guest_ioapic.c, guest_devices.sh and this document.
Existing tests and device_bus files in this package are unmodified dependencies.

The runtime integrator should instantiate these guest profiles in the private
per-guest device state, connect timer IRQ to guest LAPIC IRR, connect LAPIC EOI
to ioapic_eoi, and dispatch APIC timer MMIO/MSRs only after guest-mode validation.
Poll timer before guest entry and before live LVT writes. Keep diagnostic builds
on their old initializer. Do not enable a profile merely to report a feature
complete: the VM-exit integration and interrupt delivery must still be tested.

## Verification and limitations

`sh krnl/tests/guest_devices.sh` runs four host tests with UBSan and compiles
both changed units freestanding. This does not launch Linux, QEMU, VMX or touch
host APICs. It proves the model contracts, not hardware/guest integration.

Remaining work: VM-exit dispatch and IRQ/EOI wiring, calibrated timer frequency,
live divider changes (must cancel first), TSC deadline, full xAPIC register map,
IOAPIC arbitrary destinations/logical/lowest-priority modes, full event replay,
PIC/PIT/HPET and ACPI/firmware device enumeration. IOAPIC remains single CPU0,
24 pins, fixed delivery and rejects duplicate unmasked vectors. Linux boot and
interactive shell remain unverified; this increment is not full M2 completion.
