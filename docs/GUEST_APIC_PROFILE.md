# Opt-in BSP APIC register model, not a complete guest platform

New `guest_apic.{c,h}` composes the existing priority core and timer. The old
`lapic` diagnostic initializer/default stays unchanged. Callers must not publish
APIC CPUID capability from this model alone. EPT MMIO trapping, instruction
emulation, firmware topology, scheduling and VM-entry transactions are separate.

## Supported state

- Explicit xAPIC or x2APIC initialization, single BSP ID0, FEE00000 fixed base.
- APIC_BASE xAPIC->x2APIC transition preserving IRQ/timer state. Disable,
  relocation and back-transition reject. Mode-mismatched MMIO/MSR access rejects.
- Version 0x60014 (max LVT index 6), x2APIC LDR=1; xAPIC LDR read/write and
  flat DFR. APIC ID0, TPR/PPR/SVR, ISR/IRR/TMR, EOI and clear ESR readback.
- Seven LVT registers have reset/readback state (timer plus CMCI, thermal,
  performance, LINT0, LINT1, error). All non-timer sources MUST remain masked;
  unmask requests reject because those event sources are not implemented.
  This is NOT a claim of thermal/perf/NMI/ExtINT delivery support.
- Fixed edge ICR to physical ID0, self and all-including-self (one BSP), self-IPI.
  Other destinations/logical delivery, NMI/INIT/SIPI, lowest-priority, and
  all-excluding-self reject. The caller must account for these limitations.
- xAPIC aligned 32-bit MMIO register offsets, including ICR high destination0;
  no decoder or MMIO mapping. x2APIC-only self-IPI MMIO alias rejects.
- Timer uses the existing synthetic 1024 host-TSC tick quantum, not a calibrated
  architectural clock. Timer due events must be polled before live LVT updates.
  The guest timer profile now accepts masked low-vector reset/programming and
  masked arming, while legacy strict behavior is unchanged.

## Event ownership

External enqueue requires enabled APIC and vector32..255. IRR coalesces same
vector, TPR blocks selection not enqueue. Trigger mode must agree for pending
or in-service uses of a vector; otherwise enqueue rejects unchanged. TMR records
trigger at queue time, remains through service, and is cleared on EOI only if no
same-vector request remains pending. EOI returns `{eoi,level,vector}` so caller
can notify IOAPIC after committing the register write. Failed downstream notify
must retry only that notify, not pop another ISR entry.

`select` does not consume. `accept` requires the same highest-priority selection
and then moves IRR to ISR. Single-vCPU serialization is required between token
reservation and delivery confirmation. The dispatcher must prevent EOI/priority
writes from invalidating a reserved token and distinguish failed entry from
accepted entry and interrupted vectoring. No VM-entry transaction is implemented
here. Do not call legacy core APIs directly on the embedded core or TMR tracking
will diverge; use guest_apic_enqueue/select/accept/read/write throughout.

## Scope and tests

Host-only tests cover reset/register/mode rules, MMIO rejects, pending trigger
conflicts, EOI effects, fixed IPI, timer phase and atomic rejection. Old/guest
timer tests also pass. Freestanding compile passes. No Linux/QEMU/VMX run.
The fixed Linux kernel .config is unavailable, so this is not a verified list of
its requirements. Full ACPI, PIC/PIT/HPET, NMI, local-error events, accurate timer
calibration and real IRQ delivery remain unverified or absent.
