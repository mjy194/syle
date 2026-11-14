# APIC dependency and staged implementation

## Current observed stop / next bounded probe

The fixed OVMF executed RDMSR1b after masked-PIC setup. This does not by itself
prove it needs an enabled APIC, but the selected firmware includes LocalApic
and LocalApicTimer libraries. The next probe reuses linux_disabled_apic without
modifying it: the synthetic integrated controller exists but is globally
disabled. Read IA32_APIC_BASE returns FEE00100 (BSP and fixed base, enable/x2
clear), consistent with the actual fcp_query leaf1 APIC/x2 feature bits clear.
The adapter independently gates those bits and MSR support. It stages EAX/EDX,
commits RIP then registers; any uncertain write forbids resumption.

Writes to1b, all x2APIC MSRs and APIC MMIO still stop. There is no active LAPIC,
clock, IRQ source, IPI, EOI forwarding or host MSR access in this step. Success
of this read is NOT validation of OVMF's APIC or timer requirements. A following
MMIO/enable stop is expected evidence of the next missing dependency.

This probe also prints read-only HOST.CLOCK_CPUID max leaves and15/16/80000007
from the outer QEMU CPU for the separate clock source-admission investigation.
These values are not exposed to the inner guest and do not change time policy.

## Active LAPIC path: not yet connected

1. Use existing guest_apic, lapic and lapic_timer models, not host APIC MSRs.
   Select a distinct explicit active-xAPIC profile; CPUID APIC, MSR1b hardware
   enable/mode/base and firmware topology must agree. Keep x2APIC unadvertised
   unless its entire transition and MSR route is deliberately enabled.
2. Leave FEE00000..FEE00FFF absent from EPT. Handle only verified MMIO exits
   through a decoder with guest instruction fetch and guest page-table walk.
   Firmware instructions can live in high ROM, unlike the existing low-RAM-only
   helper; table walks and operands need owned RAM/ROM checks and bounds. Never
   turn an EPT hole into host physical access. Unsupported instruction stops.
3. Clone the APIC model, decode/register operation, commit architectural RIP/
   register result, then commit model effects. Unknown registers, modes or IPI
   destinations stop. Single-BSP topology must not pretend to have running APs.
4. Choose a documented virtual time source. Existing lapic_timer's base1024
   synthetic ticks is NOT a calibrated APIC bus frequency. Do not substitute
   host CPUID15 into the inner model without verified frequency/admission and
   a matching guest-visible clock contract. Timer reads must advance honestly,
   not return made-up countdown values to break firmware waits.
5. Initially test masked timer countdown with explicit time increments. Before
   any unmasked source, wire real IRR/ISR priority, interruptibility/window,
   event reservation/entry confirmation and EOI handling. Fail on collisions or
   uncertain entry; never consume an interrupt before confirmed guest entry.
6. Unit-test MMIO widths, offsets, guest translation, ROM fetch, state failure
   atomicity, timer deadlines/dividers, masks, pending vectors and EOI effects.
   Then run controlled register/timer/IRQ guests, tiny regression and OVMF.

The PIC-only model remains source-free. FullAPIC integration is a new device
milestone, not a reason to broaden current MSR passthrough or fake success.
