# Quiet LINT1 configuration, not NMI delivery

Real OVMF writes FEE00360=0400, matching BaseXApicLib.c's NMI, active-high,
edge, unmasked LINT1 setup. This target has no guest NMI producer, injector
or LINT1 signal router. Its host signals are a separate domain; PIC masks
and guest IF do not prove NMI absence and are NOT used to justify this flag.

A separate lint1_nmi_deasserted opt-in defaultsfalse. It may be enabled only
from an otherwise valid guard with LINT1 initiallymasked. Only slot4 exact0400
is allowed by this opt-in. LINT0's flag alone cannot authorize LINT1. Wrong
vectors/modes/polarity/trigger/reserved fields or widths stop, not normalized.
Other guard invariants (noIRR/ISR/TMR/pending/ICR, stopped dummy timer) remain.

Native verifies actual VMCS pin controls after open and before first entry:
external-interrupt exiting bit0 AND NMI exiting bit3 must be set. Pin controls
are logged. Host NMI exits are exception/NMI reason0 (NMI type2); reason0 has
no resume/injection handler and terminates observation. No host NMI is cleared
as a substitute for guest-line deassertion, or forwarded into a guest LVT.
Root-state asynchronous handling remains separate and does not gain a router.

Existing each-entry event/vectoring/pending checks and LINT0 PIC gate remain.
Any new guest NMI source/router invalidates this static profile and requires
new event ownership implementation; this is not a general NMI model.

MMIO samples time first, stages only the LINT register and publishes after
RF/RIP writes succeed. Any partial VMCS failure is terminal. Masked timer
phase/deadline do not restart. Tests cover both IF values, missing control bits,
entry/vectoring NMI, illegal forms, default/cross-slot rejection, and RF/RIP
failure. Successful NMI registration is not NMI delivery or safe NMI handling.
Native acceptance of this candidate is pending.
