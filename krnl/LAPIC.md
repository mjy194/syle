# Restricted single-vCPU x2APIC diagnostic

Build `make -C efi vmx-apic`; run `efi/tests/apic.sh`.
Default CPU profile and all previous diagnostics are unchanged.

This is an experimental x2APIC register subset, NOT an OS-compatible APIC.
Guest executes real RDMSR/WRMSR; VMX primary use-MSR-bitmaps must be ZERO,
so all such instructions exit and no guest MSR operation reaches host
hardware. A forced-on bit28 fails closed. No MSR bitmap is used. Physical
external-interrupt exiting is explicitly enabled; unexpected host IRQ
exits fail the bounded test rather than route to the guest's minimal IDT.

## Register contract

- IA32_APIC_BASE 0x1b: fixed fee00900 (enabled/BSP/xAPIC); supports only
  transition to fee00d00 (enabled/BSP/x2APIC), or idempotent writes.
- x2APIC ID 0x802=0; version 0x803=0x14 is a diagnostic value, not a claim
  of implemented LVT registers or OS compatibility.
- TPR 0x808: eight-bit read/write; PPR 0x80a: read-only.
- EOI 0x80b: write-only zero, clears highest ISR; empty ISR is a no-op.
- SVR 0x80f: vector>=32 and software-enable bit8 only.
- ISR 0x810..817 and IRR 0x820..827: read-only 256-bit bitmaps.
- Self-IPI 0x83f: write-only vector32..255 when software-enabled.

Unsupported MSRs, invalid values, MMIO mode access, and unsupported mode
transitions reject the diagnostic instead of touching host MSRs or silently
succeeding. This is not complete architectural #GP emulation for bad access.
No timer, LVT, ICR, IOAPIC, MSI, SMP, APICv, interrupt remapping or real device
source is implemented. Experimental CPUID leaf1 advertises APIC/x2APIC for
this guest test only; arbitrary CPUID leaves reject. It is not the normal
Syle CPU profile or a bootable-OS CPU contract.

## Delivery and priority

Self-IPI coalesces pending vectors in IRR. Selection does not mutate state.
The highest pending vector is eligible only when enabled and its priority
class exceeds PPR class. PPR is full TPR when TPR class >= highest ISR class,
otherwise highest ISR class with low nibble zero. EOI retires highest ISR,
allowing a previously blocked lower-priority pending interrupt.

The bounded test deliberately schedules delivery at the HLT boundary; it
is not a general instruction-by-instruction asynchronous interrupt arbiter.
IF, blocking, activity, pending injection and IDT-vectoring checks are
required in addition to virtual APIC priority. HLT is emulated with next RIP
and halted activity, then entry external vector0x40 wakes the guest.
IRR→ISR commit occurs only at the exact first handler RDMSR exit, proving
successful entry and handler execution, before emulating its ISR read.
VMfail/entry-failure/unexpected first exit never consumes pending state.
There is no concurrent CPU or external queue mutation between select/commit.

## Guest evidence

After original EPT checks, guest enables x2APIC/SVR, raises TPR to0x50,
self-IPIs vector0x40 and reads IRR/PPR to prove pending but blocked.
It lowers TPR, STI;NOP;HLT, then the private IDT40 handler validates saved
return RIP, reads ISR/PPR, writes EOI and IRETQs. Guest checks ISR/IRR empty,
counter1, then VMCALL confirms original stack, active state and IF restored.

Required hardware markers:
- APIC.TPR_MASKED_PENDING_RETAINED
- APIC.HALT_PENDING_INJECTION vector=0x40
- APIC.ACCEPT_COMMITTED IRR_TO_ISR
- APIC.GUEST_EOI
- APIC.DIAGNOSTIC_PASS registers_pending_priority_eoi_wakeup=1
- VMX.ROOT_ROUNDTRIP_PASS

Host unit tests cover nested priority, same-class blocking, PPR low bits,
highest ISR retirement, disabled SVR, coalescing, vector255, illegal accesses
and no pending consumption before successful acceptance. TCG only proves
the safe no-VMX path, not actual MSR interception or delivery.

## Hardware evidence (2026-11-09)

The independent `vmx-apic` image passed isolated archbook QEMU/KVM runs at
256 MiB and 512 MiB, one vCPU. Verified ELF SHA256:
`caabf6f148b0ee59abbf4f8b51abe43a557e5814685d2e56ad8d3c53b604681b`.
Both runs logged TPR_MASKED_PENDING_RETAINED, HALT_PENDING_INJECTION vector40,
ACCEPT_COMMITTED IRR_TO_ISR, GUEST_EOI, APIC.DIAGNOSTIC_PASS and VMX roundtrip.
Raw logs: Library `libfile_219ee7cc9c6881919073d1099dca62b5`
(`apic-evidence.tar.gz`). All test QEMU processes exited, no host settings changed.

This diagnostic arbitrates only at its HLT boundary, not arbitrary instruction
boundaries. Its synthetic CPUID profile is not an OS-compatible CPU contract
(in particular leaf1 EDX.MSR is not yet advertised); the fixed payload knows
its restricted MSR interface. Passing this test does not imply a full LAPIC,
APIC timer, IOAPIC/MSI, SMP, or Windows/Hyper-V support.

## Active interrupt-window diagnostic

`make -C efi vmx-window` / `efi/tests/window.sh` is independent of the
HLT boundary build. Primary interrupt-window exiting bit2 must support
both 0 and 1; otherwise this optional test reports BLOCKED. Every update
preserves the other primary controls. Physical IRQ exiting stays enabled.

Guest CLI+self-IPI first leaves IRR pending behind TPR. Lowering TPR makes
it eligible while IF is still zero. At that WRMSR exit the monitor arms
bit2, without consuming IRR or injecting an event. The active guest executes
a marker, STI and NOP. When the STI shadow expires, reason7 arrives at the
fixed following label in this diagnostic. The handler separately checks
reason7, IF1, no STI/MOVSS/other blocking, active activity, no pending entry
event, no valid vectoring event and LAPIC eligibility. It does not read or
interpret exit instruction length for reason7 and never advances its RIP.

The monitor clears bit2 successfully BEFORE writing the entry interruption
field. Failed control writes cannot proceed to injection. The selected vector
remains IRR pending until the exact first handler RDMSR exit proves entry
and handler delivery. Unexpected exits, VMfail or entry failure terminate
without accepting pending state. The guest handler validates saved RIP,
reads ISR/PPR, EOIs and IRETQs; resumed code records a second marker and
acknowledges count1, original stack, IF and active activity. No HLT is used
to trigger delivery in this diagnostic, and a stray HLT is rejected.

Fixed STI;NOP marker/RIP checks are test constraints, not a universal
arbitrary-guest interrupt scheduling contract. General concurrent arrival,
multiple-vCPU delivery, nested event queues and real device sources remain
unimplemented. Unit tests cover IF0, STI and MOVSS blocking, absent pending,
TPR/PPR masking, preexisting injection, activity and capability rejection.

Additional required hardware evidence:
- APIC.WINDOW_ARMED_IF0_PENDING_RETAINED
- APIC.WINDOW_READY_SHADOW_CLEAR_INJECT_SAME_RIP
- APIC.ACCEPT_COMMITTED IRR_TO_ISR
- APIC.GUEST_EOI
- APIC.WINDOW_DIAGNOSTIC_PASS active_no_hlt=1

## Active interrupt-window hardware evidence (2026-11-09)

`vmx-window` passed isolated archbook KVM runs at 256 MiB and 512 MiB,
one vCPU. Verified ELF SHA256:
`0f914c97d23f67f95faa40f9943560298ea0584f3146485a2cf85e04aacc0713`.
Both runs logged WINDOW_ARMED_IF0_PENDING_RETAINED,
WINDOW_READY_SHADOW_CLEAR_INJECT_SAME_RIP, ACCEPT_COMMITTED, GUEST_EOI,
WINDOW_DIAGNOSTIC_PASS active_no_hlt=1 and VMX roundtrip. No HLT was used
to trigger delivery. The fixed guest uses STI;NOP to clear STI shadow;
MOV SS blocking is covered by policy tests, not an executed hardware scenario.
All temporary QEMU processes exited. Logs: Library
`libfile_7b68f47a743081918a0aaa1643fe15b5` (`window-evidence.tar.gz`).
This extends the earlier HLT-only diagnostic; it remains a bounded single-event
profile, not general asynchronous event scheduling or full APIC emulation.

## MOV SS shadow and synchronous-fault collision probes

Two separate optional builds preserve the prior window diagnostic:

- `vmx-movss` / `tests/movss.sh`: after lowering TPR, host deliberately
  does NOT arm a window yet. Guest STI;NOP first finishes the STI shadow,
  then MOV SS immediately followed by a store to 0xb080. The prelude's
  already-executed code stub page at GPA0xb000 is RX in EPT; offset0x80
  does not overwrite the stub. The write causes EPT violation at original
  store RIP. Monitor requires actual VMCS MOVSS blocking exactly2, IF1,
  active state and eligible pending IRQ, with no entry/vectoring event.
  Qualification low9bits must be0x1aa (write against EPT R/X), GPA=b080.
  It changes only that owned EPT leaf RX→RWX, INVEPTs, and arms the window.
  It does NOT change fault RIP or guest interruptibility. Hardware retries
  and completes the original store, naturally consumes MOVSS shadow, then
  reason7 permits injection at the next label. Backing0xb080=0x55, marker,
  exact window RIP, IRQ count1 and EOI prove the retry and eventual delivery.
  This is not a software shadow-clear test.

- `vmx-collision` / `tests/collision.sh`: same setup but MOV SS;UD2
  replaces the store. Host records the actual #UD blocking state without
  guessing whether it must remain set. It validates exact fault RIP,
  reason0/info0x80000306 and retained IRR0x40, no pending injection or
  valid vectoring. It makes NO injection, RIP advance or IRR consumption,
  and tears down with COLLISION_EXPECTED_REFUSAL_PASS. This is a successful
  fail-closed test, NOT successful guest exception delivery/completion or
  complete exception-versus-IRQ arbitration. No generic APIC success marker
  is emitted for this refusal case.

Physical IRQs remain intercepted. Unit tests distinguish MOVSS bit1 from
STI bit0, exact EPT access/permissions/address/RIP and reject inappropriate
window delivery. Ordinary TCG does not prove either hardware scenario.

Required A markers:
- APIC.MOVSS_BLOCKING_OBSERVED=2 IRQ_NOT_INJECTED
- APIC.MOVSS_EPT_RETRY_SAME_RIP_WINDOW_ARMED
- APIC.WINDOW_READY_SHADOW_CLEAR_INJECT_SAME_RIP
- APIC.MOVSS_NATURAL_CLEAR_PASS store=0x55

Required B markers:
- APIC.COLLISION_OBSERVED_UD_BLOCKING=<actual value>
- APIC.COLLISION_SAFE_REJECT irq_pending_retained=1 no_injection=1
- APIC.COLLISION_EXPECTED_REFUSAL_PASS

## MOV SS and collision hardware evidence (2026-11-09)

Both `vmx-movss` and `vmx-collision` passed their distinct expected outcomes
on isolated archbook KVM, each at 256 MiB and 512 MiB (one vCPU).
MOVSS ELF SHA256: `95274e70c46b0a0d57443a4f08cbb689a38f2f9e347b3b5341c31a520d0a5add`.
Collision ELF SHA256: `32ddf993c42b895900238c65f1d6995d4818225215e577a9571975e1872bfd26`.
A observed VMCS MOVSS blocking=2 at the EPT write fault, retained IRQ,
remediated EPT and retried without changing RIP/blocking. CPU completed the
store (0x55), naturally cleared shadow, then window injection/EOI passed.
B observed MOVSS;UD2 exception with blocking=2, retained pending IRQ, injected
nothing and returned COLLISION_EXPECTED_REFUSAL_PASS. B is a successful
fail-closed test, NOT successful exception delivery or complete arbitration.
Logs: Library `libfile_d74170b440a0819187f9f088109dff5c`
(`movss-evidence.tar.gz`). Full cloud regression passed in
`efi/build/movss-full-regression.log`. All temporary QEMU processes exited.

## Supported MOVSS + #UD + pending-IRQ delivery

Independent `vmx-collision-deliver`, `tests/collision_deliver.sh`.
Reference: Intel SDM Vol3C, 326019-084US (June 2024), §§27.3.1, 27.7.1:
https://cdrdv2-public.intel.com/825750/326019-sdm-vol-3c.pdf

Guest-state checks forbid STI/MOVSS blocking for injected external interrupts
(type0) and NMIs (type2), not hardware #UD (type3). Vectoring VM-entry
architecturally consumes STI/MOVSS blocking. The prohibition on executing
VMLAUNCH/VMRESUME while the HOST CPU is MOVSS-blocked is a different rule.

This diagnostic accepts only the observed exact MOVSS=2, active state,
IF1, TF0, no pending debug exception, precise #UD RIP/info, no prior
injection/vectoring and pending vector0x40. It preserves the fault RIP AND
guest interruptibility field, injects hardware #UD first, and leaves IRR
unconsumed. No manual shadow-clear occurs.

Private gate6 checks the fault frame, changes saved RIP to the following
guest label, increments r13 once, then VMCALLs from inside the handler.
Host requires IF0, shadow0, expected handler stack/RIP, IRR0x40 retained,
ISR empty and count1. Only then it clears entry-valid and arms the window.
Guest IRETQ restores IF1; reason7 at the return label now permits IRQ
injection. The existing gate40 EOI/IRETQ flow proves exactly one IRQ
(r15=1), with original stack and both handler counts checked.

Unknown faults, delivery nesting, debug state, altered shadow/type/RIP,
entry failure and duplicate events fail closed. The old collision-refusal
test remains separate. This is one supported pair, not general nested
exception escalation or a complete event arbiter.

Expected markers:
- APIC.COLLISION_INJECT_UD_FIRST preserve_shadow=2 preserve_fault_RIP=1
- APIC.COLLISION_UD_HANDLER_ACK shadow=0 IF=0 IRR_retained=1
- APIC.WINDOW_READY_SHADOW_CLEAR_INJECT_SAME_RIP
- APIC.ACCEPT_COMMITTED IRR_TO_ISR
- APIC.GUEST_EOI
- APIC.COLLISION_DELIVERY_PASS ud_first=1 irq_after_iret=1

## Collision delivery hardware evidence (2026-11-09)

`vmx-collision-deliver` passed isolated archbook KVM runs at 256 MiB and
512 MiB, one vCPU. Verified ELF SHA256:
`8925cbbd3bbad34d187b42f66669e2619f204279a8be3bbe72008b78af2cd5fe`.
Both observed MOVSS blocking=2, injected #UD first without changing shadow or
fault RIP, then observed handler ACK shadow=0/IF=0/IRR retained. IRETQ restored
IF and the interrupt window then delivered IRQ once, with EOI and
COLLISION_DELIVERY_PASS ud_first=1 irq_after_iret=1. VMX roundtrip passed.
Evidence: Library `libfile_12c03bb952948191bf368118d1cfd778`
(`deliver-evidence.tar.gz`). Full cloud regression:
`efi/build/collision-deliver-full-regression.log`. Test QEMU processes exited.
This proves the bounded #UD-plus-pending-IRQ case, not arbitrary nested faults,
IDT-vectoring recovery or general exception escalation. Unsupported cases
still fail closed; the prior expected-refusal diagnostic remains available.

## One-shot virtual timer backed by VMX preemption exits

Independent `vmx-timer` / `tests/timer.sh`.
The virtual clock is monotonic host invariant-TSC elapsed time, INCLUDING
VMX-root handling time. One virtual undivided timer tick is exactly 1024
TSC ticks. This is an explicit experimental frequency contract, not physical
APIC bus frequency, milliseconds, or non-root-only runtime.

The VMX-preemption timer is only the exit alarm. SDM Vol3C §26.5.1 and
Appendix A.6 define its rate from VMX_MISC[4:0]=X: one decrement per 2^X
TSC ticks while operating in non-root (C0/C1/C2; not deeper C-states).
§27.7.4 reloads it on entry. Before EACH entry this implementation converts
the absolute remaining deadline with ceil(delta/2^X), clips to32bits and
writes VMCS482e. It never restarts the original duration at every exit.
An early/chunk alarm reloads the remaining deadline; it cannot enqueue an
IRQ before absolute expiry. No wall-clock frequency calibration is claimed.

Require invariant TSC, supported HLT activity, and both set/clear support
for pin6 and exit22. Timer activation and save-on-exit controls are changed
as a pair, with no VM-entry between writes; failure stops. Inactive timer
clears both. Reason52 must report saved countdown0. Backwards time and
deadline arithmetic overflow reject without model mutation.

Register subset:
- 0x832 LVT timer: vector32..255, maskbit16, one-shot mode only.
- 0x838 initial count: uint32; zero cancels, nonzero restarts from write time.
- 0x839 current count: RO, ceiling remaining virtual ticks, stops at0.
- 0x83e divide: only bits0/1/3; encodings0,1,2,3,8,9,A,B mean
  divisors2,4,8,16,32,64,128,1.
- LVT/divider reconfiguration while armed is unsupported and rejected;
  cancel first. No periodic, TSC-deadline, real bus-clock or LVT-status ABI.

Masked expiry disarms with current0 and no IRR. Unmasking does not resurrect
it. EOI clears ISR only and never restarts the timer. The test first polls a
masked one-shot to0 and verifies empty IRR before/after unmask. It then
rearms a longer one-shot, observes nonzero current count and executes
STI;NOP;HLT. The monitor emulates HLT's next RIP/activity and enters the
halted guest with a nonzero VMX countdown, WITHOUT injecting an event.

Only a subsequent real reason52 with saved0 and elapsed deadline can
enqueue the timer vector and inject it through the ordinary ISR/EOI/IRETQ
path. If the deadline expires before the halted entry, the test reports
NOT_EXERCISED rather than pretending a timed HLT wakeup. The monitor tracks
virtual HLT independently of the physical wake induced by the alarm;
early alarms retain virtual halt and rearm without advancing RIP. Saved
activity must be active or halted, and external injection still requires
IF1, no blocking/pending/vectoring. Delivery remains a bounded single-vCPU
experiment, not an OS clock or scheduler.

Required hardware evidence:
- APIC.TIMER_VMX_TSC_SHIFT=<hardware X>
- APIC.TIMER_MASKED_EXPIRED_NO_IRR
- APIC.TIMER_GUEST_HALTED_AWAIT_REAL_EXPIRY
- APIC.TIMER_PREEMPTION_EXIT saved=0
- APIC.TIMER_DEADLINE_REACHED_IRQ_FROM_HALTED_GUEST
- APIC.ACCEPT_COMMITTED IRR_TO_ISR / APIC.GUEST_EOI
- APIC.TIMER_DIAGNOSTIC_PASS masked=1 oneshot_irq=1 real_halt_expiry=1

Unit tests cover every divider, masking, cancel/restart, readonly/reserved
access, backward time/overflow, ceil/clipping/rate boundaries and capability
pairs. TCG cannot prove the real preemption-timer expiry.

## One-shot timer hardware evidence (2026-11-09)

`vmx-timer` passed isolated archbook KVM at 256 MiB and 512 MiB, one vCPU.
ELF SHA256: `63230a888a31eaaa1b7a45b77104cee0c3a3f7c6fb6c5ed1ad5950e17ac35d1a`.
Default `-cpu host` correctly reported NO_INVARIANT_TSC. A native read-only
CPUID probe confirmed host 80000007.EDX=0x100; only the temporary test VMs then
used `-cpu host,+invtsc`, without changing host configuration.
Both observed VMX timer shift=5, masked expiry without IRR, entry into halted
state awaiting actual expiry, reason52/saved=0, TSC at or after deadline,
IRQ delivery, EOI and TIMER_DIAGNOSTIC_PASS real_halt_expiry=1.
Logs: Library `libfile_ea12112ff04881919d71f2b27e4978fe`
(`timer-evidence.tar.gz`), including initial blocked runs and host CPUID proof.
Temporary QEMU processes exited. This validates an absolute invariant-TSC
one-shot diagnostic clock including root handling time, not calibrated
wall-clock seconds, periodic mode, timer migration or a full scheduler.

## Optional periodic timer profile

Independent `vmx-periodic` / `tests/periodic.sh`.
Only `lapic_timer_init_mode(..., true)` enables LVT mode bit17; the original
one-shot initializer and diagnostic still reject periodic mode. Bit18
(TSC-deadline) and other unsupported LVT bits remain rejected. Changing
LVT/divider while armed still requires stopping first.

For period P=initial*1024*divide and the previous deadline D, polling at
now>=D computes k=floor((now-D)/P)+1 and advances to D+kP. It never uses
now+P, so delayed handling does not move the phase. Arithmetic overflow or
backward time rejects atomically. Current-count is ceiling ticks to the
next phase; reading it does not consume an expiry. Initial0 stops, a nonzero
rewrite starts a new phase from that write time.

One poll returns at most one IRQ request even if k periods elapsed.
`last_expirations=k` is diagnostic metadata, not a guest register. Pending
requests coalesce in the existing IRR bit; a vector can be pending while
the same vector is in ISR, but PPR blocks acceptance until EOI. EOI never
reloads or changes the timer phase. Masked expiries still advance phase but
produce no IRR; unmasking later cannot resurrect them. Stop does not erase
an already pending IRR request.

Guest first observes a masked periodic current-count wrap, stops and checks
IRR0, then unmasks and starts a new period. It performs three HLT waits,
three handlers/EOIs and checks current-count while active between waits.
There is no initial-count rewrite between these three interrupts. On the
third return it writes initial0, reads current0 and acknowledges completion.

Success requires three genuine HLT/preemption-timer expiry wakes, not just
three software-injected vectors; all original saved-count0/deadline checks
apply. Late active expiries are retained/coalesced rather than dropped and
can wake the next HLT immediately, but if this prevents observing all three
real halted expiry waits the diagnostic ends NOT_EXERCISED, not success.
This remains a bounded single-vector source, not a general timer scheduler.

Additional hardware markers:
- APIC.PERIODIC_ELAPSED_PERIODS=<k>
- APIC.PERIODIC_NEXT_PHASE_TSC=<phase deadline>
- APIC.PERIODIC_IRQ_ACK_COUNT=1,2,3
- APIC.PERIODIC_DIAGNOSTIC_PASS irq_eoi=3 phase_preserved=1

Unit tests cover delayed/missed periods without phase drift, coalescing,
mask consumption, cancel/rewrite, current-count phase, overflow and old
profile rejection. Hardware trace records deadlines and observed TSC values.

Periodic injection safety: before timer polling or handling any preemption exit,
any outstanding IRQ injection must be acknowledged by the exact first handler
RDMSR exit with no valid IDT-vectoring event. An intervening reason 52 (or any
other unrecognized exit) stops the diagnostic without another VM entry; it does
not guess that delivery completed or reenter with the stale entry-valid bit.
IRR remains uncommitted on this refusal. This is deliberately fail-closed, not
support for arbitrary interruption of event delivery.

## Periodic timer hardware evidence (2026-11-09)

The final, injection-ack-guarded `vmx-periodic` image passed isolated archbook
KVM at 256 MiB and 512 MiB, one vCPU, `-cpu host,+invtsc`.
ELF SHA256: `f02a25a28eae86de5dd5332918a5a54c37cda67d5a86452cf4e7e753504960fe`.
Each run observed three real HLT/preemption-timer-expiry/IRQ/EOI cycles and
PERIODIC_DIAGNOSTIC_PASS irq_eoi=3 phase_preserved=1. Masked periods advanced
without IRR; actual masked-phase logs included elapsed-periods=2, retaining
original phase rather than rearming from processing time. Final initial=0
stopped the timer. Evidence: Library
`libfile_78ee8e7462888191b7a0761d857d2625` (`periodic-evidence.tar.gz`).
All QEMU test processes exited. Clock remains absolute host invariant-TSC,
including root time; this is not calibrated wall time or migration support.

Review fixed a duplicate-injection hazard: while awaiting the first confirmed
handler exit, reason52 or any unexpected exit now fails closed before timer
polling or reentry, retaining the event rather than reusing entry-valid.

## Restricted device/MMIO vertical slice (`vmx-ioapic`)

This independent diagnostic adds `device_bus`, `mmio_decode`, `ioapic`, and
`device_guest`. It is not a real peripheral or an OS-compatible platform.
The fixture maps guest linear/physical 0xe000 to an experimental IOAPIC aperture,
not the conventional 0xfec00000. Guest paging admits the page while EPT has no
RAM leaf; actual guest MOV accesses cause EPT violations. The generic bus is
address-range based and does not know guest RIPs. Registration rejects overlap
and arithmetic overflow. Accesses are aligned four-byte, single-page only.

The decoder reads actual bytes from the private executable guest code page.
It supports 89/8b MOV r32/m32, one REX prefix (R/X/B only), ModRM/SIB, signed
displacements, and RIP-relative addresses. Other opcodes, widths, prefixes,
noncanonical/overflowing addresses and truncated instructions fail closed.
RSP comes from VMCS; other GPRs come from the assembly save area. Reads zero
extend r32; stores use the low 32 bits. A private four-level supervisor page
walk checks every table backing and permission before comparing decoded EA,
VMCS GLA, translated GPA and the EPT access qualification. No guest address is
used as a host pointer. LA57, large/user pages and other paging profiles are
not supported. Device side effects occur only after complete validation;
subsequent VMCS write failure stops without retrying a completed store.

IOREGSEL and IOWIN expose 24 redirection entries, fixed physical destination
CPU 0, fixed delivery, active-high edge/level and mask. Unsupported fields,
widths/registers are rejected. Entries with remote IRR cannot be rewritten;
two unmasked pins may not share a vector. These are explicit subset limits.
An edge rising while masked is discarded. An asserted unmasked level sets
remote IRR only when enqueue into LAPIC IRR succeeds. Deassert does not revoke
an already queued/in-service interrupt. EOI is routed using the actual highest
ISR vector being cleared. It clears the matching remote IRR and requeues if
the line is still asserted and unmasked. This device-delivery acknowledgement
is distinct from the later guest-entry acknowledgement that moves IRR to ISR.

The guest performs real MMIO at multiple RIPs, verifies register reads, then
requests explicitly synthetic host fixture stimuli via VMCALL. One edge pulse
causes one interrupt. One level assertion causes two: the first EOI requeues
while asserted; the second handler requests deassert before EOI, preventing a
third. Both use real interrupt-window exits and guest IDT/IRETQ. The strict
first-handler acknowledgement remains before any further event processing.
The VMCALL command sequence only controls test input and final assertions; it
does not emulate MMIO based on RIP or invent a physical device.

Run `source scripts/cloud-env.sh; tests/ioapic.sh` from `efi/`. This runs
ASan/UBSan bus/IOAPIC/decoder unit tests, builds the separate ESP and verifies
TCG's honest VMX-unavailable skip. Actual EPT/MMIO/IRQ evidence requires KVM;
cloud compilation/TCG alone must not be described as hardware success.

The MMIO emulator additionally rejects all interruptibility blocking, TF
single-step and pending-debug state before touching a device. Advancing RIP
alone does not emulate shadow consumption or debug-exception delivery; those
states require a future fuller instruction-emulation contract.
