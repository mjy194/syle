# N18: two physical guest CPUs, one AP mailbox

`make smp`, `tests/smp.sh`; ESP `build/smp-esp`. Unlike VMX smoke tests,
TCG can exercise this path: QEMU `-smp 2` executes a genuine separate AP
CPU context. KVM validation is separate; this is not multiple nested vCPUs.

Boot ABI v4 adds an optional AP trampoline allocation range. Only the SMP
loader target allocates one EFI LoaderData page below 1 MiB before the final
GetMemoryMap/ExitBootServices transaction. No MP Services callback survives
ExitBootServices. PMM already permanently reserves all memory below 1 MiB;
the kernel additionally verifies the range is within an owned WB LoaderData
descriptor. Non-SMP loaders leave this range zero.

The kernel checks bounded RSDP/RSDT/XSDT/MADT tables and checksums through a
memory-map-aware reader. Exactly two enabled unique APIC IDs, including the
BSP, are required. This profile supports xAPIC mode and IDs below 256, not
x2APIC mode, hotplug or arbitrary topologies. MADT local APIC address must
match IA32_APIC_BASE. The local APIC mapping must previously be absent;
a single UC mapping is installed, never a UC alias of existing mapped RAM.

BSP copies/patches the relocation-free 16/32/64-bit trampoline before making
that page RX. The AP gets the owned kernel CR3 and private stack. Temporary
GDT accessed bits are preset so AP does not modify its RX trampoline.
CR0.CD/NW are cleared after WBINVD, WP is enabled, and AP validates its PAT
slot0 WB/slot3 UC contract. AP then installs its own GDT, busy TSS descriptor,
IDT and normal/double-fault/NMI IST stacks. It does not call the BSP's
exceptions_init or logging. AP faults release-publish a private record and
halt; there is a short pre-IDT startup window where failures can only time out.

BSP targets the discovered AP ID with INIT assert/deassert and two SIPIs;
there is no broadcast. PIT channel2 one-shots provide 10 ms and 200 us delays,
not a guessed TSC frequency. Every timer, delivery and acknowledgment wait
has a bound. PIT channel2 is owned exclusively during this isolated test;
its control/count programming is not restored for an OS/device consumer.

After AP reports its CPUID APIC ID, BSP sends three mailbox sequence numbers
with payloads. Release/acquire publication orders each request and response.
Only BSP allocates memory and prints output. AP confirms parking then halts
with IF=0. All AP resources remain reserved even after success, and especially
after any timeout: a late AP must never use freed pages. No AP task scheduler,
IPI rendezvous, cross-core INVEPT, per-CPU VMX or non-WB cache transition has
been implemented by this milestone. These are follow-on work, not inferred
from successful mailbox exchange.

Pure ASan/UBSan tests validate malformed ACPI, topology constraints, low
bootstrap layout and failure atomicity. Two-CPU TCG has reached
`SMP.N18_PASS actual_AP=1 mailbox=3 parked=1 resources_retained=1`.

## N18 actual KVM evidence (2026-11-09)

Isolated archbook QEMU/KVM `-smp 2`, 256 MiB and 512 MiB both passed
`SMP.N18_PASS actual_AP=1 mailbox=3 parked=1 resources_retained=1`.
BSP APIC ID 0 started AP ID 1; that distinct execution context returned
mailbox sequence ACKs 1, 2, 3 before parking. This is two real guest logical
CPU execution contexts backed by KVM, not a single-vCPU mailbox simulation.
It does not claim fixed host physical-core affinity, per-CPU VMX, IPI-based
rendezvous, or cross-CPU INVEPT. All temporary QEMU processes exited.
Kernel SHA256: `a219b62c3037bbe3166493afd23d3a6e443953786eccc4087d7cdc1236f9d216`.
EFI SHA256: `9105eb53765e76552db53aab4a2d2b219080aba6d867cefc7d4e3cf123944849`.
Both artifacts are required because boot v4 preserves the low trampoline page
before ExitBootServices. Evidence Library
`libfile_1389a169a750819186f5b9b2a9d87dd6` (`smp-evidence.tar.gz`).

Independent full cloud regression passed: `efi/build/smp-review-full.log`, `FINAL_STATUS=0`, including actual two-CPU TCG AP startup plus all prior non-IOAPIC suites.

## N19: interrupt-driven AP rendezvous

`make smp-ipi`, `tests/smp-ipi.sh`, ESP `build/smp-ipi-esp` retain N18 as a
separate target. The BSP publishes each command and sends a targeted fixed
IPI vector 0x40. Only the real AP interrupt handler computes the response;
the idle main loop never polls/executes command work. The handler verifies
its APIC ID, exact next sequence and the local APIC ISR bit for vector 0x40,
then completes work, writes EOI and verifies the ISR bit cleared before
release-publishing ACK. Duplicate, out-of-order or non-ISR calls stop safely.

AP installs vector 0x40 on private IST4. The assembly wrapper saves all GPRs,
aligns the C stack, clears DF for C and restores the original flags/stack
with IRETQ. The AP masks implemented LVT sources, checks APIC max-LVT 5/6,
sets TPR0 and enables SVR before publishing online. Unknown vectors,
including unexpected spurious vector0xff, are diagnostic fail-closed events;
they are not incorrectly acknowledged as vector0x40. No shared logging or
allocator runs from the handler. Only AP enables IF.

Idle uses CLI/check-stop followed by adjacent STI;HLT. STI's interrupt
shadow covers HLT, so an early pending IPI is not lost in a check/sleep gap.
The fourth IPI asks the main loop to park; its handler ACK and the eventual
main-loop parked flag are separate. This is a bounded diagnostic protocol,
not a general scheduler, reliable arbitrary-message queue or completed
cross-core EPT shootdown. N20 will establish independent per-CPU VMX state;
actual shared-EPT warm/update/invalidate proof must follow separately.

Two-CPU TCG passes with four real fixed IPIs, ISR/EOI checks and final park.

## N19 actual fixed-IPI KVM evidence (2026-11-09)

Archbook isolated KVM `-smp 2`, 256 MiB and 512 MiB both passed
`SMP.N19_PASS actual_fixed_IPI=4 EOI_before_ACK=1 HLT_idle=1 parked=1`.
Kernel SHA256: `b9d5c42b0686399f6faa057b23fb0b5d71e703023822b60a975180504e84b769`.
EFI SHA256 remains `9105eb53765e76552db53aab4a2d2b219080aba6d867cefc7d4e3cf123944849`.
AP ID 1 handled four real fixed IPIs, including stop. The ISR checks vector
0x40 in service, does the mailbox work, issues EOI and checks the ISR cleared
before publishing ACK; the HLT idle loop does not execute that work itself.
All temporary QEMU processes exited. Evidence Library
`libfile_037bd0ffc40c819190e7dc44cc74c0a2` (`smp-ipi-evidence.tar.gz`).
This is not yet per-CPU VMX or cross-CPU EPT invalidation evidence.

N19 independent full regression passed: `efi/build/smp-ipi-review-full.log`, `FINAL_STATUS=0`, including two-CPU TCG and all previous non-IOAPIC suites.

## N20: independent per-CPU VMX root contexts

`make smp-vmx`, `tests/smp-vmx.sh`, ESP `build/smp-vmx-esp`. BSP allocates
separate below-4GiB VMXON pages before AP startup. No AP allocator, static
shared VMX execution context or active VMCS is used. Each CPU reads its own
CPUID, FEATURE_CONTROL, VMX BASIC, fixed-bit MSRs and WB memory policy.
FEATURE_CONTROL must already be locked with outside-SMX VMX enabled; the
code never writes it. Only required CR0.NE and CR4.VMXE may be added. All
fixed requirements are validated, original VMXE1 is rejected, IF0 is required.

BSP enters VMX root, then the first real IPI enters AP VMX root. Both remain
in root simultaneously. The second IPI executes AP-local VMPTRST, then BSP
executes its own VMPTRST: each must report no current VMCS (all ones). The
third IPI executes AP-local VMXOFF and restores its saved control registers;
BSP exits locally afterwards. Restoration is read back and compared. The
fourth IPI parks AP. Only after both CPUs exited and AP parked does BSP free
the two VMXON regions. AP bootstrap/stack resources remain reserved.

Every context records its owner APIC ID. Check and leave reject another CPU.
VMXOFF failure leaves root_live set and does not restore CR4 or free memory.
Any other uncertain active-root failure stops the test and retains resources.
This is not a generic recovery mechanism or a claim that a guest ran:
there is no VMCS, VMLAUNCH, shared EPT, INVEPT or cross-core translation-cache
verification in this target. Such work requires a separate warm/update/
acknowledged-invalidation experiment.

TCG has no VMX here: it must emit `SMP.N20_BLOCKED CPUID_VMX0` before any VMX
MSR/instruction, never N20_PASS. Pure fixed-bit/region policy tests and build
run in the cloud; actual two-CPU VMX execution is a separate KVM acceptance.
N19's error EOI was tightened to acknowledge only when ISR40 is actually set.

## N20 actual per-CPU VMX KVM evidence (2026-11-09)

Archbook isolated KVM `-smp 2`, 256 MiB and 512 MiB both passed
`SMP.N20_PASS distinct_CPU_VMXON=2 local_VMPTRST=2 local_VMXOFF=2 no_VMCS=1`.
Kernel SHA256: `371b5d553c182b1e95d630a4cef3cf7b6582efe2902bd24c7f55b52b78d46f78`.
EFI remains the N18 boot-v4 image. BSP ID 0 and AP ID 1 were simultaneously
in VMX root using distinct VMXON regions. Each locally verified VMPTRST had
no current VMCS; each completed VMXOFF and restored/read back its own CR0/CR4.
This still does not launch guests or warm EPT on either CPU.
Evidence Library `libfile_34ab076f60908191aab8231ee658e73d`
(`smp-vmx-evidence.tar.gz`). All temporary QEMU processes exited.

N20 independent complete cloud regression passed: `efi/build/smp-vmx-review-full.log`, `FINAL_STATUS=0`. TCG correctly blocks VMX, while KVM evidence above verifies root transitions.

## N21: actual guests on two CPU-owned VMCSs

`make smp-guest`, `tests/smp-guest.sh`, ESP `build/smp-guest-esp`.
Each CPU receives its own VMCS page, host snapshot and GPR/CR2 frame. The
assembly entry wrapper keeps frame pointers and host RSP anchors on the
calling CPU's stack, including the AP's private IPI IST. There is no use of
the old global vmx_guest_regs or host-CR2 slots. VM-exit records guest GPRs
before restoring local CR2, callee-saved registers and host flags. Local
GDTR/IDTR limits are restored and host CRs, FS/GS, PAT/EFER/SYSENTER are
checked against the per-CPU snapshot.

BSP preallocates a shared immutable EPT tree, code and read-only data. Guest
paging entries have A/D preset and their EPT pages are read-only, so parallel
readers cannot race on shared guest paging updates. Guest stack GPA9000 and
GPAa000 map distinct private pages, with stack tops a000/b000. Each real
payload pushes/pops its CPU-specific R8 token into R9, reads shared GPA8000
and executes VMCALL. The host verifies reason, RIP, instruction length,
RSP, RAX, both tokens, actual APIC owner and both stack backing words.

The AP runs its own guest from the second IPI handler and clears its VMCS
before acknowledging; BSP then runs its own guest. Both remain in VMX root
until the existing local-OFF sequence. A local initial INVEPT prevents stale
reused-EPTP translations, not a cross-core update. All shared data remains
unchanged. Final VMCS clear and both VMXOFFs precede freeing, and AP must be
parked before resource release. Unknown exits, entry failures or failed
clear stop with resources retained. This is trusted diagnostic execution,
not arbitrary guest isolation or a general scheduler.

N21 demonstrates two actual processors using the same EPT and distinct
contexts; it does not yet demonstrate a changed mapping and distributed
shootdown. N22 must warm, modify, invalidate on each CPU, wait for completion
and verify the new mapping separately. Cloud pure tests/build/TCG-blocked
checks do not substitute for KVM guest execution acceptance.

## N21 actual per-CPU guest execution evidence (2026-11-09)

Archbook isolated KVM `-smp 2`, 256 MiB and 512 MiB passed
`SMP.N21_PASS actual_guest_CPUs=2 shared_EPT_read=2 private_stacks=2 host_restore=2`.
Kernel SHA256: `ee8d3c5765e09c18287881667382d982d82a311d5df24808b7a89dfa687ab19a`.
Both BSP and AP ran their own VMCS and real guest load/VMCALL, reading
0x1122334455667788 through the same physical EPT root, with private guest
stacks and verified local host-state restoration. Runs are sequentially
coordinated, not simultaneous guest execution; reading proves translation use
but does not prove how long any hardware cache entry remains resident.
No mapping update/shootdown is included in this milestone.
Evidence Library `libfile_28dc13af21388191991aaa3b09be76d8`
(`smp-guest-evidence.tar.gz`); all temporary QEMU processes exited.

N21 independent full cloud regression passed: `efi/build/smp-guest-review-full.log`, `FINAL_STATUS=0`.

## N22: retained guests and real two-CPU EPT shootdown

`smp-shootdown` retains each owner's active VMCS after its first real guest
read/VMCALL. It does not CLEAR/rebuild or implicitly invalidate before the
second entry. Both guests first read `1122334455667788` through the same EPTP.
While both guests are stopped in root, BSP release-publishes a replacement
GPA8000 leaf backed by another preallocated WB page (`8877665544332211`). BSP
executes local single-context INVEPT; a directed fixed IPI makes AP execute
its own INVEPT. AP release ACK is published only after that instruction and
EOI complete. BSP acquire-waits before permitting either guest's second run.
Both use VMRESUME, advancing only the validated prior VMCALL, and independently
read the replacement value. VMCS objects are finally cleared locally before
VMXOFF. Both old and new backing remain owned until both CPUs OFF and AP parked.

Failure/timeout stops and retains resources; no failure ACK is treated as
success. This is a bounded two-owner rendezvous, not a general running-vCPU
scheduler or concurrent page-table mutation API. We do not require stale data
before invalidation to be observable: hardware is free to discard caches.
The explicit per-CPU invalidation and real warm/reentry evidence, rather than
an incidental stale-value comparison, establish the tested sequence.

Cloud `tests/smp-shootdown.sh` builds and checks TCG capability rejection only;
actual VM entry/shootdown requires isolated two-CPU KVM validation.

## N22 actual cross-CPU EPT invalidation evidence (2026-11-09)

Archbook isolated KVM `-smp 2`, 256 MiB and 512 MiB both passed
`SMP.N22_PASS warmed_CPUs=2 actual_INVEPT_CPUs=2 resumed_CPUs=2 retained_VMCS=1`.
Kernel SHA256: `dd7e58265eca24268520a105ca779e6972f1755a9c84dbebd2da6e589c3b43fb`.
Both CPUs first read 0x1122334455667788 through the shared EPT. Their VMCSs
remained current/launched; after replacement of the data leaf, BSP performed
local INVEPT and AP performed its own INVEPT inside a real IPI request before
ACK. Only then did both VMRESUME, each reading 0x8877665544332211.
There is no hidden CLEAR/reinitialize/initialization-INVEPT between reads.
The log BOTH_VMPTRST_NONE is the saved pre-warm check, not a statement that
AP has no current VMCS after warming. Old backing remains retained until safe
cleanup. This verifies a coordinated, root-paused two-CPU protocol, not yet
arbitrary concurrently running guest interruption or multi-vCPU nested EPT.
Evidence Library `libfile_da185193552c819189a527c3ea45ca09`
(`smp-shootdown-evidence.tar.gz`); all temporary QEMU processes exited.

N22 independent full regression passed: `efi/build/smp-shootdown-review-full.log`, `FINAL_STATUS=0`, including all prior non-IOAPIC suites.

## N23: bounded epoch-based shootdown controller

`shootdown.c` is a reusable pure, single-writer policy (BSP coordinator only).
A fixed participant mask covers every CPU that may cache the EPTP. Generation
starts at 1. Entry marks a participating CPU running only in READY at the exact
generation; a real successful VM exit is required before leave. Begin requires
all participants stopped, checks generation overflow, and closes the entry gate
before publication. Publication moves to WAIT. Only successful local INVEPT
results with matching CPU/generation are acknowledged. Duplicate, stale, future,
and unknown-CPU ACKs do not change the state. All mask bits are required before
READY reopens; actual failure poisons the controller and retains all resources.
This policy is not itself a concurrent lock or an IPI transport implementation.

The two-CPU adapter owns the policy on BSP; AP reads an operation+generation
command after acquiring the transport request. The IPI handler performs the
operation and publishes matching completed generation before its release ACK.
BSP acquires that ACK before recording the controller flush ACK. Transport
sequence prevents replay but does not select the operation. Both entries are
explicitly authorized by the controller, and real return is required to leave.

Three actual mapping publications select B/A/B backing. Each epoch has BSP and
AP INVEPT, then two retained-VMCS VMRESUMEs and exact read-value checks. All old
backing is kept through all epochs and until both local VMXOFF and AP park.
The actual failure path closes the gate and stops; it does not recover or free.
No running-guest forced exit, permission-fault round, dynamic participant change,
or arbitrary nested concurrent mutation is claimed. Hardware stale residency
before invalidation is not assumed. `tests/smp-controller.sh` provides pure
negative tests and TCG capability rejection; actual rounds require KVM.

## N23 epoch controller hardware evidence (2026-11-09)

Archbook isolated KVM `-smp 2`, 256 MiB and 512 MiB passed
`SMP.N23_PASS generations=3 participants=2 gated_resumes=6 retained_VMCS=1`.
Kernel SHA256: `35fd59dce3dffbce5114b15dc2632193108e307b9c0913899ca17d8250ef1470`.
Three publish/flush/ACK generations (2,3,4) completed, with both CPUs reading
0x8877665544332211, then 0x1122334455667788, then 0x8877665544332211.
Every reentry used the controller gate and preserved VMCS/VMRESUME; operation
and epoch travel through real IPI commands, not fixture sequence dispatch.
Stale/duplicate/unknown ACK and overflow are pure policy negative tests, not
injected hardware failures. The runtime remains BSP-single-writer with both
CPU guests root-paused during mapping publication, not arbitrary concurrent
nested-EPT mutation or forced exit of running guests.
Evidence Library `libfile_9618ffb3d4588191aa40def772e41a13`
(`smp-controller-evidence.tar.gz`); all temporary QEMU processes exited.

N23 independent full regression passed: `efi/build/smp-controller-review-full.log`, `FINAL_STATUS=0`.

## N24: running guest kick before quiescence

`smp-kick` moves **all AP guest work out of the vector40 IPI handler**. That
handler queues an operation and completes EOI/transport ACK; AP main executes
it with IF0 and publishes a separate work-completion generation only on success.
The idle loop uses CLI/check-work/STI;HLT. Thus the old command ISR is no longer
in service while guest code runs, and a second IPI cannot be blocked by it.

The AP guest reads old GPA8000, publishes old value and heartbeat on its own
RW communication page, then loops with IF0 until a stop flag. BSP observes the
heartbeat and sends one directed fixed vector41 kick. Physical external-interrupt
exiting is set and exit-control acknowledge-interrupt-on-exit bit15 is capability
gated. Guest IF0 does not mask external-interrupt VM exits under this control.
Only actual reason1, valid external vector41, IF0, expected loop RIP and owner
register/stack evidence are accepted. AP main verifies local ISR41, issues EOI,
and verifies ISR41 clear before releasing quiesced work completion. Unexpected
root vector41 uses the existing fail-closed IDT fault path, never a fake reason1.
No kick is injected into the guest.

BSP leaves the controller running state only after that real completion, begins
the new epoch, publishes replacement leaf, and obtains local+remote INVEPT ACKs.
Only then it release-writes the stop flag and authorizes AP reentry. VMRESUME
continues the interrupted RIP without advancing it. Guest naturally leaves its
loop, reads the new backing, and VMCALLs. BSP also resumes and checks new data.
Both retained VMCS objects are cleared before local OFF and final resource free.
Any timeout/unknown exit/vector/failed flush closes the entry gate and retains.
This is one trusted loop and one kick, not arbitrary interrupt arbitration or a
production concurrent scheduler. Heartbeat alone is never treated as quiescence.

## N24 actual running-guest kick evidence (2026-11-09)

Archbook isolated KVM `-smp 2`, 256 MiB and 512 MiB passed
`SMP.N24_PASS running_guest_kick=1 actual_reason1=1 quiesce_before_publish=1 flush_CPUs=2`.
Kernel SHA256: `36cedad5fa191fe3b457c362b8c0a9afbf4066c91abbbc70bf310f1a98397e41`.
AP guest ran with IF=0 and published a heartbeat. A real fixed IPI 0x41 caused
VM-exit reason 1 with interrupt info 0x80000041; root checked ISR ownership,
completed EOI, then acknowledged quiescence. Only afterward did BSP publish
the EPT update and complete both CPUs' INVEPT acknowledgments. AP resumed at
its interrupted RIP; AP and BSP both read 0x8877665544332211 from new backing.
Evidence Library `libfile_2e598e4165d0819197b595bdaace3cae`
(`smp-kick-evidence.tar.gz`); all temporary QEMU processes exited.
This is one owned kick vector and one running AP guest, not unrestricted
interrupt routing, dynamic CPU topology, or full multi-vCPU nested execution.

N24 independent full regression passed: `efi/build/smp-kick-review-full.log`, `FINAL_STATUS=0`.

### N31A hardware evidence (2026-11-09)

Kernel `8e9c8553d3841aa2ed5b00468fc38021dd2b4cda908429bf9c6e283c1ac89301`
passed isolated archbook QEMU/KVM `-cpu host -smp 2`, 256/512 MiB, exit 33.
BSP/AP IDs 0/1; actual running-guest kick exit reason1/vector0x80000041;
shared CR3 0x100000 and PAT 0x0007040600070406; both CPUs acknowledged local
INVLPG and WBINVD at epoch2. Both resumed guests read original WB value
0x1122334455667788. `N31A_PASS` explicitly records `type_transition=0`.
No temporary QEMU remained. Evidence Library:
`libfile_4fa17d01763c81919e92f10e760755a0`.
These are two executed KVM guest logical CPUs, not a host affinity claim.
The result proves the maintenance rendezvous only, not cross-CPU type changes.

N31A final independent full regression: `efi/build/smp-maintenance-review-full.log`,
`FINAL_STATUS=0`, 58 marked tests plus console-memory. Final kernel unchanged.

### N31B hardware evidence (2026-11-09)

Kernel `327f21f13e2862deb968152eb145bfafacb8381623d65f61efa4583d40e869d1`
passed isolated archbook KVM `-cpu host -smp 2`, 256/512 MiB, exit 33.
Epochs2/3 each logged all six phases with both CPU acknowledgments. Host and
EPT changed WB-to-UC, both guests read 0x1122334455667788; reverse conversion
restored WB and both read the same value before clear/off/park/free.
`N31B_PASS WB_UC_WB=1 actual_guest_CPUs=2 all_phase_ACK=2 restored_before_free=1`.
The actual EPT target alias count was one; additional alias/failure cases are
pure-test coverage, not claimed as additional hardware mappings here.
Evidence Library: `libfile_57c3887decd881918d93e028484c4d64`.
No temporary QEMU remained. Scope: trusted direct guests, one shared EPTP,
UC, two KVM guest logical CPUs. Not nested-registry SMP or arbitrary guest
CR/PAT changes, not host physical-core affinity, not WT/WC/WP SMP support.

N31B final independent full regression: `efi/build/smp-cache-transition-review-full.log`,
`FINAL_STATUS=0`, 59 marked tests plus console-memory, final kernel unchanged.
