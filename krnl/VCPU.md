# Bounded vCPU state and synchronous exception injection

`make -C efi vmx-events` creates a separate `build/vmx-events-esp`.
Existing vmx-guest, vmx-ept, vmx-exits paths are unchanged.

## Seven-exit trusted diagnostic

1. Normal private EPT-backed data access, VMCALL acknowledgement.
2–4. Existing absent-read, read-only-write, NX-execute EPT violations:
validate and remediate permissions with INVEPT, retry the same RIP.
5. Guest UD2 triggers exception-bitmap exit reason 0. Validate exact expected
RIP, active state, no interruptibility blocking, no pending injection or valid
IDT-vectoring event. Require exit interrupt info exactly 0x80000306:
valid=1, hardware-exception type=3, vector=6, error-code valid=0.
6. Reenter at unchanged fault RIP with VM-entry interruption info 0x80000306.
Entry error-code and instruction-length fields are zero, not a UD2 skip.
Guest-private IDT gate 6 (selector 8, 64-bit interrupt gate, IST 0) invokes
a copied position-independent handler. It checks saved fault RIP, advances
the *saved frame* by two bytes, sets an acknowledgement marker, increments
a once-only counter and executes IRETQ. It does not manufacture a host frame.
Long-mode frame includes RIP/CS/RFLAGS/RSP/SS even at same CPL; no error code
is pushed for #UD. Handler push/pop preserves RAX and IRETQ consumes the CPU
frame. The following VMCALL proves the return path and count=1.
7. HLT exit transitions RUNNABLE → HALTED → COMPLETED without reentry.

The injection-valid field is explicitly cleared after successful reentry
and its subsequent exit. Entry failure faults the diagnostic and tears down;
it never retries a pending event. Unknown exit, wrong frame/rip/marker,
duplicate event, or mismatched info ends FAULTED. Hardware exceptions do
not depend on guest IF, which remains clear in this diagnostic.

## Boundaries

This is NOT a scheduler or general event queue. HALTED is a recorded terminal
diagnostic state, not implemented HLT sleep/wakeup. External interrupts,
interrupt-window exiting, virtual APIC, NMI, timer injection, multi-vCPU,
double-fault escalation, arbitrary guest handlers and nested virtualization
are deferred. All exceptions remain intercepted; only the expected single
#UD is accepted. A fault during guest event delivery is rejected.

Guest IDT and TSS live at private GPA/GVA 0x5800/0x5100 in the fixture's own
descriptor page; gate handler targets guest code at GPA 0x6000 plus PIC offset.
No host descriptor pointers are exposed. This trusted fixture is not a
complete untrusted-guest security boundary.

## Verification

`efi/tests/vcpu.sh`: ASan/UBSan policy and negative state/input tests,
build and QEMU TCG safe VMX-unavailable path. Cloud TCG cannot validate
VM-entry exception delivery. KVM/VMX evidence must additionally contain:

- VCPU.UD_VALIDATED_INJECT_SAME_RIP
- VCPU.GUEST_UD_HANDLER_IRETQ_ACK
- VCPU.HALTED
- VCPU.COMPLETED
- VCPU.EVENT_DIAGNOSTIC_PASS ud_injected=1 ud_handled=1 hlt=1
- VMX.ROOT_ROUNDTRIP_PASS

No hardware claim is made merely from compiler/unit/TCG success.

On 2026-11-09 two isolated archbook QEMU/KVM runs (Core Ultra 9 185H,
one vCPU, 256 MiB and 512 MiB, fresh OVMF variables) produced all markers
above, all three EPT permission-retry passes, and STAGE0.SMOKE_PASS.
Observed #UD exit RIP was 0x6056, acknowledgement VMCALL RIP 0x6066,
and terminal HLT RIP 0x606e. Both test VMs exited; host configuration and
other VMs were not changed. Raw logs: Library
`libfile_e11c345de6f481919166d6e6005f2036` (`events-evidence.tar.gz`).
This is nested KVM execution evidence, not bare-metal or Hyper-V validation.

## Synthetic HLT wakeup increment

Independent `make -C efi vmx-wakeup`, `efi/tests/wakeup.sh`.
This adds two exits to the original seven: first HLT, IRQ-handler VMCALL
acknowledgement, final HLT after CLI. Guest executes STI; NOP before the
first HLT, ensuring IF=1 and the STI interrupt shadow has expired.

HLT-exiting intercepts before HLT executes. The monitor validates length=1,
expected RIP, current activity=active; explicitly advances RIP by one and
writes guest activity=halted. It then provides a deterministic **synthetic
host test event**, vector 0x40. No wall-clock delay or asynchronous host
interrupt is claimed. Injection requires IF=1, no STI/MOVSS/other blocking,
halted activity, no pending injection and no valid IDT-vectoring event.
Blocked or duplicate attempts fail closed, not queued for interrupt-window
delivery. Entry interruption info is 0x80000040 (valid, external type0);
no error code or software instruction length.

The monitor leaves guest activity HALTED for VM-entry injection to wake it.
Guest-private IDT gate 0x40 targets a PIC handler that validates the saved
RIP equals the instruction following HLT, increments r14 once and IRETQs.
Following VMCALL must prove r14=1, activity=active, IF=1 and original stack.
Only then does software state become RUNNABLE. Final CLI; HLT ends
HALTED→COMPLETED, without another injected event.

Before configuring this optional diagnostic, IA32_VMX_MISC bit 6 must
advertise VM-entry support for halted guest activity. Otherwise the test
reports VCPU.WAKE_BLOCKED NO_HLT_ACTIVITY_SUPPORT and never attempts entry.
Other diagnostic builds do not gain this requirement.

Pin-based external-interrupt exiting is explicitly required for this build
so IF=1 cannot route real host IRQs to the minimal guest IDT. A physical
interrupt exit is unexpected and rejected; no real IRQ/APIC handling is
implemented. Host IF stays disabled in the bounded monitor.
VM-entry failure faults the diagnostic, never replaying pending events.
Successful subsequent exit clears VM-entry injection-valid.

Expected additional evidence:
- VCPU.HALTED_SYNTHETIC_IRQ_QUEUED vector=0x40
- VCPU.IRQ_HANDLER_IRETQ_ACK activity=active count=1
- VCPU.WAKE_DIAGNOSTIC_PASS synthetic_irq=1 wake_ack=1

This validates CPU event-delivery mechanics, not device interrupts, APIC,
scheduler, multi-vCPU wakeups or nested virtualization.

## HLT wakeup hardware evidence (2026-11-09)

The first `vmx-wakeup` build passed an isolated archbook KVM run with one
vCPU and 256 MiB: HLT at RIP 0x6078, synthetic vector 0x40 queued with
halted VMCS state, guest IRQ-handler/IRETQ acknowledgement at RIP 0x6084,
active state and count=1, final HLT at RIP 0x608d, WAKE_DIAGNOSTIC_PASS,
EVENT_DIAGNOSTIC_PASS and VMX root roundtrip. Three EPT retries also passed.
Raw logs remain on archbook at `/tmp/syle-wakeup-20261109/wakeup.log`.

Independent review then added a wakeup-only IA32_VMX_MISC bit6 HLT-activity
capability gate. Final ELF SHA256:
`1e6bc89437351fa5bdded91507e260ddc105c8a4698513bbd471cede127defa1`.
The final build passed unit/sanitizer/build/TCG and gate review. Archbook
went offline before final-image 512 MiB hardware retest; that retest is
pending, not claimed. Final transfer artifact:
Library `libfile_83eb4fe395b48191b86362fe711fb257`.

## Nonzero-error-code #GP increment

Independent `make -C efi vmx-gp`, `efi/tests/gp.sh` retains EPT and #UD
prelude, then executes MOV DS with selector 0x28. Guest GDT limit remains
39 (0x27), so this selector is out of bounds and produces #GP(0x28).
There is no CR2/#PF claim.

The monitor requires exact fault RIP and VM-exit interruption information
0x80000b0d: valid, hardware exception type3, vector13, deliver-error-code.
It reads the actual exit error code from VMCS 0x4406 and requires exactly
0x28, rejecting EXT/TI/high-bit changes. Entry error field 0x4018 receives
this validated value; entry info is 0x80000b0d. Fault RIP is unchanged.
Pending injection, IDT vectoring, blocking, non-active state, unknown reason
and duplicate delivery fail closed. After successful reentry/exit the entry
valid and error fields are cleared before subsequent resume.

Private IDT gate13 uses selector8, IST0, type0x8e. Guest handler pushes RAX,
checks error at RSP+8 and saved fault RIP at RSP+16. It replaces saved RIP
with the assembly label immediately after MOV DS (no guessed instruction
length), increments r15 once, restores RAX, removes the eight-byte error
slot, then IRETQ consumes the normal RIP/CS/RFLAGS/RSP/SS frame. VMCALL
after return must prove original stack, exact return label and r15=1.
HLT ends the diagnostic. Nine exits total, no host-generated error-frame
substitute and no attempt at general exception escalation.

Required hardware evidence:
- VCPU.GP_VALIDATED_INJECT_SAME_RIP error=0x28
- VCPU.GP_HANDLER_IRETQ_ACK error_frame_removed count=1
- VCPU.GP_DIAGNOSTIC_PASS error=0x28 handled=1
- VMX.ROOT_ROUNDTRIP_PASS

Unit tests include exact error-code propagation, missing error-valid,
wrong error (including high bits), wrong RIP/type/vector/state, pending
injection, IDT vectoring and duplicate acknowledgement rejection.

Update: archbook reconnected on 2026-11-09. The exact final ELF hash above
was verified after materialization and passed the 512 MiB isolated KVM run:
WAKE_DIAGNOSTIC_PASS, EVENT_DIAGNOSTIC_PASS, VMX roundtrip and smoke pass.
Final-image hardware retest is now complete. Evidence:
Library `libfile_3ef38dba6b248191bf9b6f1af6064920` (`wakeup-evidence.tar.gz`).

## #GP hardware evidence (2026-11-09)

The independent `vmx-gp` diagnostic passed two isolated archbook KVM runs,
one vCPU with 256 MiB and 512 MiB. Materialized ELF hash:
`3b5c4555c8b1a980a8533e2021c6fd6cde42e6dc7dd62af9239b74f57344b02d`.
Both showed actual #GP(0x28), same-RIP reinjection with the error code,
guest error-frame removal / IRETQ acknowledgement count=1, GP_DIAGNOSTIC_PASS,
existing #UD/EPT passes and VMX roundtrip. All temporary QEMU processes exited.
Raw logs: Library `libfile_48cb885ed6e8819185a4270aaaf17eb6`
(`gp-evidence.tar.gz`). Cloud full regression: `efi/build/gp-full-regression.log`.
This does not implement #PF/CR2, double-fault escalation, or general OS support.

## Guest #PF and CR2 increment

Independent `make -C efi vmx-pf`, `efi/tests/pf.sh`, with a PF-specific
entry-assembly object. Existing builds use the original entry object.
The EPT/#UD prelude is followed by a supervisor data read at GVA 0xc000.
Guest PTE[12]=0x8002 has P=0. Its target bits name GPA 0x8000, whose EPT
mapping/backing remain valid, but a nonpresent PTE never translates those
target bits. All guest page-walk GPAs have valid RW EPT mappings. This
produces guest #PF, not an EPT denial. Error code is exactly zero
(nonpresent, read, supervisor, data, no reserved-bit violation).

Monitor validates reason0, interrupt info 0x80000b0e, exit error0 and exit
qualification 0xc000. For intercepted #PF, VM-exit qualification carries
the faulting guest linear address; VMCS guest-linear-address field is not
used. CR2 must not be assumed updated by the intercepted fault. The
monitor explicitly sets saved guest CR2 from validated qualification
before same-RIP injection with error-code-valid and entry error0.

CR2 is NOT a VMCS-switched field. PF-specific assembly saves host CR2 and
loads saved guest CR2 immediately before entry. On every VM exit it saves
guest CR2 and restores host CR2 before returning to C. VMLAUNCH/VMRESUME
VMfail also restores host CR2; early VMWRITE failure occurs before the
switch. Tests set a host CR2 sentinel 0x12345000 immediately around each
entry, verify its restoration, and restore the original caller CR2 before
any C failure/cleanup handling. This is bounded, interrupts-disabled,
single-vCPU code, not a general NMI-safe context-switch implementation.

Private gate14 handler verifies error0, CR2=0xc000 and saved fault RIP.
It changes saved RIP to the following assembly label, removes the error
slot and IRETQs. This test skips the known failing read, not demand-pages
it. Following VMCALL verifies single count, restored stack and persistent
guest CR2=0xc000; host's original CR2 is preserved. Final HLT completes.

Hardware evidence required:
- VCPU.PF_VALIDATED_INJECT_SAME_RIP error=0 cr2=0xc000
- VCPU.PF_HANDLER_IRETQ_ACK error_frame_removed count=1 host_cr2_preserved
- VCPU.PF_DIAGNOSTIC_PASS error=0 cr2=0xc000 handled=1
- VMX.ROOT_ROUNDTRIP_PASS

Unit/TCG success alone does not prove hardware #PF or CR2 switching.

## #PF / CR2 hardware evidence (2026-11-09)

The independent `vmx-pf` image passed isolated archbook KVM runs at 256 MiB
and 512 MiB (one vCPU). Verified ELF SHA256:
`569b58937d204c06ded650977099a1a1dcd8ada445697103b852524734a11fdf`.
Both runs produced PF_VALIDATED_INJECT_SAME_RIP error=0 cr2=0xc000,
PF_HANDLER_IRETQ_ACK error_frame_removed count=1 host_cr2_preserved,
PF_DIAGNOSTIC_PASS, EVENT_DIAGNOSTIC_PASS and VMX roundtrip/smoke passes.
Guest nonpresent-page read is distinguished from earlier EPT faults;
guest handler skips the test fault using its saved frame, not demand paging.
All temporary QEMU processes exited. Logs: Library
`libfile_611a4d54ae388191aefa042d0012bebb` (`pf-evidence.tar.gz`).
Full cloud regression passed: `efi/build/pf-full-regression.log`.
This covers one supervisor read #PF(error=0), not every PF error-code class,
page-table repair, fault escalation, or a general guest OS.

## Guest demand-mapping retry increment

Independent `make -C efi vmx-demand`, `efi/tests/demand.sh` reuses the
PF-specific CR2 entry wrapper but leaves the older skip-PF build unchanged.
The guest #PF handler validates old PTE exactly 0x8002 at GVA 0x4060, writes
0x8003 (present, writable, supervisor), then INVLPGs GVA 0xc000. Saved fault
RIP is untouched: removing the error slot and IRETQ retries the original
read. The following guest code checks RAX=0x42, fault count=1, CR2=0xc000,
and PTE permissions. Host ACK also checks actual guest page-table backing,
preexisting EPT leaf unchanged, backing data still 0x42 and original stack.
PTE Accessed may be set; Dirty must remain clear for this read-only access.
Writing the PTE through GVA 0x4060 affects that page's own translation dirty
bit, not the repaired target PTE dirty bit.

INVLPG-exiting must be disabled; a forced-on control reports
VCPU.DEMAND_BLOCKED INVLPG_EXITING_FORCED before entry. No implicit handling
of INVLPG exits is claimed. All other original EPT/#UD/CR2 negative handling
and teardown checks remain active.

This is guest-controlled on-demand mapping of PREALLOCATED physical memory.
The host only injects #PF, never fixes the guest PTE. There is no new EPT
allocation/remediation, page allocator, swap, general OS pager or concurrency.

Hardware success additionally requires:
- VCPU.DEMAND_RETRY_ACK read=0x42 fault_count=1 guest_pte=present_rw_supervisor
- VCPU.DEMAND_DIAGNOSTIC_PASS retry_original_load=1
- original PF/CR2 and VMX roundtrip success markers

## Guest demand mapping hardware evidence (2026-11-09)

`vmx-demand` passed isolated archbook KVM runs at 256 MiB and 512 MiB,
one vCPU. Verified ELF SHA256:
`ba90910909e78e31a718de547931076bb8d9e625a00f6e3f8dde8c6ec5129641`.
Both runs showed DEMAND_RETRY_ACK read=0x42 fault_count=1
with present/RW/supervisor guest PTE, preserved host CR2,
DEMAND_DIAGNOSTIC_PASS retry_original_load=1, PF/EVENT and VMX roundtrip passes.
The guest itself made its PTE present, executed INVLPG and retried the original
load; host EPT backing was preallocated and unchanged. This is demand mapping,
not a dynamic page allocator or full guest operating system. All temporary
QEMU processes exited. Logs: Library
`libfile_90d64b87e158819193989019983bec13` (`demand-evidence.tar.gz`).

## Interrupted IDT delivery with EPT fault

Independent `vmx-vectoring` / `tests/vectoring.sh`, based on #GP(0x28).
Reference: Intel SDM 326019-084US §§28.1, 28.2.4, 28.2.5:
https://cdrdv2-public.intel.com/825750/326019-sdm-vol-3c.pdf

After the original #GP exit is validated, the monitor temporarily removes
the owned descriptor/IDT page GPA0x5000 EPT leaf (all permissions zero),
INVEPTs, and injects the original #GP. Gate13's read at GPA0x58d0..0x58df
must then cause reason48 with IDT-vectoring valid/type3/vector13/error-valid.
The interrupted-event error code comes from VMCS 0x440a after checking the
vectoring field 0x4408; current-exit interruption fields are NOT used.
IDT-vectoring bit12 is undefined, so validation ignores that bit and builds
a clean entry field 0x80000b0d rather than copying it blindly.

Require exact saved fault RIP, original RSP/RFLAGS, active state, no shadow,
read-access EPT qualification with R/W/X denied, expected GPA range and
error0x28. Restore exactly the original owned RW EPT leaf and INVEPT;
reinject that recovered event once at the unchanged RIP. No exit instruction
length is used for this delivery fault. A subsequent real guest handler ACK
with count1 proves successful delivery; the first interrupted entry alone
does not mark the original exception handled.

SDM notes interrupted event delivery can leave stack writes even while
saved CS/RIP/RSP/RFLAGS remain unchanged. This fixture deliberately faults
on the IDT gate READ before stack-frame construction. It does not implement
general partial-stack rollback, nested #DF/NMI/software-event recovery or
arbitrary vectoring retries. Any other event, field, repeated delivery
fault or failed invalidation terminates safely with backing retained until
normal VMX/EPT cleanup. The loop allows exactly one additional entry.

Unit tests cover undefined bit12 normalization, wrong type/vector/error/
RIP/GPA/access, reserved bits, exact leaf revoke/restore, duplicate
permission changes and failed INVEPT keeping allocations live.

Required hardware evidence:
- VCPU.IDT_ACCESS_REVOKED_BEFORE_GP_INJECTION
- VCPU.IDT_VECTORING_EPT_GP info=<actual>
- VCPU.IDT_VECTORING_ERROR=0x28
- VCPU.IDT_VECTORING_GPA=<within gate13>
- VCPU.IDT_VECTORING_REINJECT_SAME_RIP error=0x28
- VCPU.GP_HANDLER_IRETQ_ACK error_frame_removed count=1
- VCPU.VECTORING_DIAGNOSTIC_PASS gp_reinjected_once=1

## IDT-vectoring recovery hardware evidence (2026-11-09)

`vmx-vectoring` passed isolated archbook KVM runs at 256 MiB and 512 MiB,
one vCPU. ELF SHA256:
`1381a1ac8ea0aaa331274976be9836713eea4d448eaea83cae4220b3e5dc0ca8`.
Both observed EPT fault GPA 0x58d0 while reading #GP gate13,
IDT-vectoring info=0x80000b0d and vectoring error=0x28. Restoring the owned
EPT leaf and INVEPT allowed same-RIP event reinjection; guest handler count
was exactly one and VECTORING_DIAGNOSTIC_PASS/VMX roundtrip passed.
Logs: Library `libfile_398cc51fce8481919a034cbbf0828e58`
(`vector-evidence.tar.gz`). Full cloud regression passed in
`efi/build/vectoring-full-regression.log`. Temporary QEMU processes exited.
This covers the known IDT-gate read before stack writes; partial stack writes,
arbitrary event/vectoring combinations and exception escalation remain unsupported.

## Strict partial-stack-write observation and replay

Independent `vmx-stackpart` / `tests/stackpart.sh`, with two additional
fresh, private WB RAM pages at GPAs0xc000/0xd000. Earlier tests use the
original stack; immediately before #GP the guest switches to aligned
RSP0xd020 without writing either new page. The six-slot #GP frame spans
0xcff0..0xd01f. All words begin with distinct address-derived sentinels.

Sources: Intel SDM Vol3C §28.1 (326019-084US) says interrupted delivery may
write stack memory while saved CS/RIP/SS/RSP/RFLAGS remain unchanged.
This is NOT a rollback or guaranteed-write-order promise. Vol3A §6.14.2
specifies 16-byte stack alignment and long-mode SS:RSP frame slots.
Vol3B's RF event-frame rules set saved RF for fault-class #GP:
https://cdrdv2-public.intel.com/843857/253669-sdm-vol-3b-dec-24.pdf

Before injection the monitor verifies both pages still untouched, makes
only the lower page read-only in EPT and INVEPTs. At the resulting delivery
write fault it requires GPA within the lower two frame slots, write/RO
qualification, exact vectoring #GP/error0x28, original fault RIP and
RSP/RFLAGS, CS8/SS16, active state and no blocking.

It then scans BOTH full pages:
- lower page must remain entirely sentinel;
- upper frame slots may contain only their distinct sentinel or the exact
  expected CS/RFLAGS(with RF)/RSP/SS;
- every other upper word must remain sentinel;
- at least one upper slot must actually have changed to the correct value.

Zero changed words logs STACKPART_NOT_EXERCISED and stops without repair
or success. Unknown memory changes fail closed. Only proven partial writes
permit restoring the lower RW leaf, INVEPT and one same-event/RIP/RSP
reinjection. No memory is rolled back or cleared. Identical writes target
private ordinary RAM with unchanged descriptors/mappings, making this
particular repeated write safe; this is not a general restart theorem.

The special guest handler uses NO pushes/scratch stack stores. It checks
all six frame slots, saved RF, original fault RIP/RSP and selectors; updates
only saved RIP to the return label, counts once, drops the error code and
IRETQs. Host verifies the complete final frame plus every surrounding
sentinel. Previously valid upper writes must still equal their original
correct values. Existing VMX teardown restores all allocated pages.

CET-active hosts are rejected for this build. No IST, CPL change, MMIO,
shared/DMA-accessible test backing, concurrent vCPU, NMI/software event or
mutable descriptor state is supported. No asynchronous writer is enabled
in this trusted no-device diagnostic. Unknown combinations still fail closed.

Hardware success requires actual evidence:
- VCPU.STACKPART_FRESH_PAGES_LOWER_WRITE_REVOKED
- VCPU.STACKPART_OBSERVED_UPPER_SLOTS=<nonzero>
- VCPU.IDT_VECTORING_REINJECT_SAME_RIP error=0x28
- VCPU.STACKPART_FRAME_CANARIES_VERIFIED
- VCPU.STACKPART_DIAGNOSTIC_PASS observed_partial_replay=1

Unit tests simulate valid/invalid partial snapshots, full frame/canaries and
INVEPT failure retention. TCG alone cannot establish a real partial write.

## Partial exception-stack replay hardware evidence (2026-11-09)

`vmx-stackpart` passed isolated archbook KVM runs at 256 MiB and 512 MiB,
one vCPU. Verified ELF SHA256:
`8e6810b6f9382899a56637fccaf98f82d47da6fea620273c0e179b7ab16f5f18`.
Both actually observed FOUR completed upper-page stack slots before the lower
page write failed at GPA 0xcff8. Vectoring info was 0x80000b0d/error0x28.
After verified original architectural state and partial contents, restoring
owned EPT write permission allowed same-event replay; complete six-slot frame,
two-page canaries and single handler count passed. This was not NOT_EXERCISED.
Logs: Library `libfile_1b047252f1948191b61677cebffde4e6`
(`stack-evidence.tar.gz`). All temporary QEMU processes exited.
Scope remains fixed same-CPL #GP on dedicated WB private RAM, no concurrent
writers, MMIO, CET, IST or privilege switch. It is not transaction rollback
or a general recovery mechanism for arbitrary partial side effects.
