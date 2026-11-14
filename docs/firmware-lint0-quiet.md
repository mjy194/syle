# Quiet LINT0 configuration, no ExtINT delivery

Real OVMF writes LINT0 FEE00350=0700. Matching BaseXApicLib.c:648-657
selects ExtINT, active-high, edge, unmasked. This is not a fixed-vector IRQ;
an asserted ExtINT path would require PIC acknowledgement and vector delivery.
Those actions are NOT implemented. IF=0 is not proof that an event is absent,
and the following source-level NMI configuration cannot be justified by IF.
Only LINT0 exact0700 is admitted in this candidate; LINT1 remains unsupported.

The nondefault clock observer has no LINT0 signal producer or router. Its PIC
is the mask/ICW-only model, RTC advances status bits without IRQ callbacks,
and no PIT/UART/external IRQ producer is connected. No pending queue is
silently drained. This static platform contract must be re-reviewed if any
producer is introduced; it is not permission to discard a runtime signal.
Unhandled external-interrupt/NMI/exception exits still stop, without injection.

The opt-in flag lint0_extint_deasserted defaults false in every guard object.
Only this observer enables it with a stable quiet PIC. It checks masterFF orFB,
slaveFF, both ICW phaseszero and lpic_entry_ok(IF1), irrespective of guestIF.
Every MMIO and entry rechecks the condition; entry also rejects event occupancy,
pending debug and (after a previous exit) vectoring. Startup does not read an
undefined exit-vectoring field before first guest entry. Changed source/route
ownership cannot retain the contract. No NMI source or NMI suppression added.

fag_valid only exempts LINT0 slot3 with exact0700; every other LVT still must be
masked, and IRR/ISR/TMR/pending-level stay empty. The default register-only
profile and Linux remain unchanged. The gate can be remasked using existing
validated register semantics, but this adds no delivery behavior.

MMIO clones configuration, samples admitted time first, clears RF on completed
MOV as before, then publishes only after RIP commit. A failed RF/RIP write is
terminal and retains old LINT configuration. The masked periodic timer retains
its separately sampled phase/deadline; unmasked expiry still stops.

Tests: default rejects0700, opted-in exact value/readback, wrongslot/mode/value,
all PIC mask combinations and ICW transitions, RF/RIP failures, armed timer
state preservation. New native diagnostics log PIC masks/phases beside every
MMIO. Native acceptance of this candidate remains pending; no claim of ExtINT,
NMI or general interrupt support is made.
