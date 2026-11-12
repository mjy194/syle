# Nested virtualization milestones

## N0: software VMX state and VMCS12 only

`nested_vmx.h/.c` is a pure, single-vCPU, one-VMCS software model. It is not
connected to the guest execution loop and does not execute L1/L2, VMXON,
VMWRITE, VMLAUNCH or VMRESUME on hardware. No existing diagnostic image or
hardware capability advertisement changes in this milestone.

The trusted adapter registers two distinct page-aligned private guest-physical
regions with a 32..52-bit physical address width. `NVMX_REVISION` is a synthetic
revision, not the host VMCS revision. The model never dereferences a supplied
address. N1 must translate instruction operands from guest virtual to guest
physical to validated private backing, read the actual revision, enforce CPL,
CR4.VMXE/CR0/FEATURE_CONTROL and instruction exception ordering, and distinguish
memory faults from VMfail. N0's #UD result for operations outside VMX operation
assumes the adapter already handled those checks; #GP is reserved for that
adapter. Unknown registered-area/profile access is UNSUPPORTED, not a host
memory access or invented architectural physical-address failure.

States include virtual VMX off/root, current pointer, initialized/active,
clear/launched, prepared entry and running L2. A fresh VMCS needs VMCLEAR before
loading; merely writing its revision does not establish clear launch state.
VMPTRST returns all ones successfully when no VMCS is current. VMCLEAR clears
launch state and disassociates the current pointer, but preserves VMCS12 data.
VMXOFF is not VMCLEAR: launch state/data are retained internally, but an active
VMCS is marked poisoned and cannot be loaded again until VMCLEAR. This avoids
claiming Intel guarantees usable VMCS contents after leaving VMX without
clearing it. Normal tested lifecycle is VMCLEAR before VMXOFF. No speculative
error 6 implementation is provided.

VMsucceed clears CF/ZF and also PF/AF/SF/OF. VMfailInvalid sets only CF among
these flags. VMfailValid sets only ZF and updates the current VMCS's read-only
instruction-error field. Other RFLAGS bits are preserved. Without a current
VMCS, VMfail(error) becomes VMfailInvalid. Successful instructions preserve the
old instruction-error field. Guest ordinary VMWRITE cannot change that field.
Architectural errors tested are 2/3 (VMCLEAR), 4/5 (entry launch-state), 7/8
(early entry validation), 9/10/11 (VMPTRLD), 12 (reserved field encoding), 13
(read-only field), and 15 (VMXON in virtual root).

VMCS12 metadata explicitly lists selectors, basic guest/host IA-32e state,
controls and read-only exit fields. Access widths are 16/32/64/natural(64 for
this profile). A 64-bit field's high-half encoding reads/writes its high 32
bits; high writes preserve low bits. Real architectural fields omitted from
the whitelist return UNSUPPORTED, not error 12 merely because N0 lacks them.
Reserved encoding bits do produce error 12. Control values are stored as data,
not validated or passed to a physical VMCS. EPTP/VPID/APICv and MSR lists are
not exposed as supported facilities. N1 must validate all entry-relevant
values and construct physical VMCS02 itself.

VMLAUNCH/VMRESUME return ENTRY_REQUEST without changing flags or declaring
success. A prepared entry prevents intervening operations. A validated real
entry can commit the transition to launched/running; cancellation preserves
launch state. Early controls/host-state failure uses VMfailValid 7/8. Guest-
state failure is **not** an early VMfail and is not implemented by N0. It needs
N1's entry-failure exit reflection. `return_from_l2` is only a lifecycle hook,
not an implementation of architectural VM-exit state loading.

Run `efi/tests/nested0.sh`: pure ASan/UBSan tests only. There is deliberately no
N0 boot image or hardware-success claim.

## N1 architectural plan (separate from N0)

L1 executes actual VMX instructions. VM exits for VMCLEAR/VMLAUNCH/VMPTRLD/
VMPTRST/VMREAD/VMRESUME/VMWRITE/VMXOFF/VMXON (basic reasons 19..27) are decoded
and dispatched to the virtual model. VMCS01 carries L1. VMCS02's physical host
state always belongs to L0, never copied from L1's virtual host-state fields.
Validated VMCS12 guest state initializes L2. On L2 VMCALL, L0 records virtual
exit state and resumes L1 at VMCS12's virtual host RIP/RSP/state. General
registers are not automatically restored by VM exit; they must follow Intel's
actual transition semantics, not a snapshot-of-L1 fiction. L1 VMRESUME then
reenters L2. L0 owns EPT; there is no advertised L1 EPT, nested EPT composition,
Hyper-V compatibility, arbitrary untrusted guest or full VMX capability claim.

Sources: Intel SDM Vol.3C 326019-084US, §§25.1/25.11.1 (VMCS lifecycle),
§§27.1–27.3 (entry checks/failures), §31.2 (VM instruction result flags),
VM-instruction error-number table; instruction-specific exceptions require
Vol.2 and will be checked in N1.
https://cdrdv2-public.intel.com/825750/326019-sdm-vol-3c.pdf

### N1 policy/decoder first increment

`nested_operand` decodes actual VMX opcodes into the N0 operation enum. It
cross-checks basic exit reasons 19..27, uses actual register operands and
64-bit ModRM/SIB addressing, and rejects unsupported forms. VMREAD/VMWRITE
currently support register-register only. VMXON/CLEAR/PTRLD/PTRST support
single-page m64. Instruction operand registers are not inferred from a fixed
RIP. Memory translation and VM-exit instruction-info checks still belong to
the future hardware adapter; decoding alone is not architectural execution.

`nested_profile` constructs a strict low-GPA IA-32e profile table for L1 to
load using real VMWRITE. Its values never contain L0 physical addresses. The
monitor checks every supported control, guest and virtual-host field against
that contract before constructing VMCS02. Guest RIP allows only the two trusted
L2 payload entry points. Virtual controls do not expose EPT or secondary
features. Validation distinguishes bad controls, bad virtual host and bad guest
state; the adapter must not map a guest-state failure into an early VMfail.
The profile fixes guest CR3 to 0x1000, descriptors to private GPA 0x5000/0x5100,
L1 stack top to 0x8000 and L2 stack top to 0x9000. The latter's backing must be
allocated/writable before any future hardware test. CR0/CR4/EFER/PAT are trusted
contract inputs, not unvalidated L1 values. This narrow profile is not a
validator for arbitrary protected-mode/real-mode guests.

Run `efi/tests/nested1-policy.sh` for pure decoder/profile and N0 regressions.
The first increment deliberately contained no hardware adapter; the following
increment implements the restricted diagnostic, still pending hardware proof.


### N1 hardware adapter increment (awaiting real KVM validation)

`nested_guest.c` and `nested_payload.inc` now build the separate `vmx-nested`
image. The L1 guest really executes VMXON/CLEAR/PTRLD, loops over the private
bootstrap table with VMWRITE, then executes VMLAUNCH. L2 has two VMCALL sites.
On each real L2 exit, the monitor confirms N0's prepared entry and records
VMCS12 guest RIP/RSP/RFLAGS and exit reason/length. It switches to VMCS01 and
reconstructs the strict virtual host-load state in its **guest fields**. L1
VMREADs the exit, advances L2 RIP using the reported instruction length and
executes VMRESUME. After the second exit L1 clears its virtual VMCS, VMXOFFs
and reports completion. General registers remain the actual L2 exit values.

VMCS02 is a distinct L0-allocated, WB-checked physical page. Its physical
controls, EPTP and host-state fields are copied from the trusted physical 01
configuration, never the guest bootstrap table. The entry assembly supplies
L0 host RIP/RSP for whichever VMCS is current. Only validated VMCS12 guest
fields are installed into 02. L0-owned EPT maps fourteen private guest RAM
pages; synthetic VMXON is at d000, VMCS12 at e000, bootstrap at c000. There is
no L1 EPT or guest control of host physical mappings. Cleanup VMCLEARs 02,
restores current 01 and releases the physical 02 page before the existing
outer VMX/EPT teardown. Unknown exits and any physical entry/setup failure
stop rather than being misreported as an L1 VMfail.

The adapter cross-checks memory VMX operand decoding against VM-exit instruction
info/qualification (RIP-relative qualification already incorporates next RIP),
requires flat DS/SS and aligned single-page m64, validates private guest paging
and checks CPL zero. TF, interruptibility blocking and pending debug/vectoring
are rejected. CPUID exposes a synthetic minimal VMX profile. RDMSR exposes
only synthetic feature-control/basic/VMX-control/fixed-register values; it never
passes through arbitrary host MSRs. The payload is trusted and intentionally
small; unsupported instruction faults and malformed guest state are fail-closed,
not full architectural exception emulation.

Run `source scripts/cloud-env.sh; tests/nested1.sh` from efi. Pure sanitizer
checks/build/TCG skip are not proof of nested execution. The expected hardware
marker is `NESTED.N1_PASS launch=1 resume=1 reflected_vmcall=2`; it must be
accompanied by the actual L1 VMX exits and two L2 exit-18 records.

## N1 hardware evidence (2026-11-09)

The independent `vmx-nested` image passed isolated archbook KVM runs at
256 MiB and 512 MiB, one vCPU. Verified ELF SHA256:
`bc1ac31c2a2fe5f04e107ef6a8dec9bde8804c72afafa14213592edf2c03900f`.
Both logs show real L1 VMXON/VMCLEAR/VMPTRLD/VMWRITE/VMLAUNCH exits,
L2 VMCALL reason18, reflection to L1, real VMREAD/VMWRITE/VMRESUME exits,
a second L2 VMCALL and reflection, followed by L1 VMCLEAR/VMXOFF.
Required marker: NESTED.N1_PASS launch=1 resume=1 reflected_vmcall=2.
Evidence: Library `libfile_628e38453ad881919bc344197cddab61`
(`nested-evidence.tar.gz`). All test QEMU processes exited.
Independent non-IOAPIC regression passed in `efi/build/nested1-review-full.log`.

Syle is the logical L0 under the outer KVM test harness; software virtualizes
its L1 VMX state and switches physical VMCS01/02 for L1/L2. This does not
require KVM to expose a third hardware nesting level. L0 owns all hardware
host state and EPT. The fixed trusted L2 only changes RIP/RSP/RFLAGS, the
state currently copied back to VMCS12; other state-save/general guest code,
L1-owned EPT composition, Hyper-V compatibility and SMP are not implemented.
Paused IOAPIC code was not part of this validation.

## N2: restricted nested EPT diagnostic (hardware evidence pending)

`vmx-nested-ept` is separate from N1. L1 really builds four EPT12 pages at
L1 GPAs 0x10000..0x13000, VMWRITEs EPTP12=0x1001e and the advertised restricted
EPT controls, and launches L2. L0's immutable EPT01 fixture resolves every
EPT12 table read and leaf L1 GPA to private allocator-owned backing. No L1
physical address is dereferenced as an HPA. `nested_ept` supports four levels,
4 KiB pages, WB leaves, no A/D, no large pages, no execute-only and W=>R.
It ANDs EPT12 permissions across all levels and then with EPT01 permissions.
Results distinguish L1 denial, L0 backing denial and malformed/unsupported
input. The hardware resolver relies on the fixture's immutable upper EPT01
levels and verifies the actual EPT01 leaf/backing identity; it is not an
arbitrary mutable EPT01 walker.

`nested_ept_hw` allocates four separate physical EPT02 pages. This first
profile fully prebuilds readable L2 pages 1..15 before entry. Nonpresent/no-read
EPT12 mappings are classified by the pure walker but cause this bounded
prebuilder to reject the plan; arbitrary unmap/lazy-fill handling is not
implemented. All leaves are validated into a temporary plan before publishing.
No guest runs concurrently. A failed physical INVEPT forbids reentry; cleanup
first clears VMCS02, then invalidates EPT02 before freeing. Repeated invalidate
failure panics and retains the potentially cached pages.

The actual alias test maps L2 GPA 0xf000 to L1 GPA 0x8000, initially read-only
in EPT12 while EPT01 backing is writable. L2 executes a store. Only the exact
supported direct-data write violation (GLA valid, not a paging-structure walk)
is reflected as reason 48. The reflected permission bits come from the cached
EPT12 effective permissions, not EPT02/L0 permissions. VMCS12 records L2 GPA,
GLA, qualification and actual guest RIP/RSP/RFLAGS. No fault instruction is
skipped, and no fault-length semantics are invented. Unknown faults, missing
L0 backing and unsupported walk-related faults stop instead of masquerading
as L1 EPT violations. No internal lazy-fill exits occur in this prebuilt
profile, so each supported L2 exit corresponds to one actual requested entry.

L1 verifies the fault, changes its leaf to writable, executes a **real INVEPT
instruction** (decoded from bytes, not identified by RIP) and VMRESUMEs at the
original store. Virtual INVEPT accepts type 1 and the current exact EPTP12;
malformed operands use error 28 with normal current-VMCS rules, while other
architecturally valid EPTPs are explicitly unsupported rather than falsely
labeled malformed. It rebuilds EPT02 and performs physical single-context
INVEPT before reporting success. L1 table writes alone never refresh the cache.
Only the explicit invalidation publishes a new plan. This is not general
concurrent EPT coherence or SMP shootdown support.

After retry, a real L2 VMCALL must find backing L1 GPA 0x8000 changed to 0x77
and the distinct L1 GPA 0xf000 backing still holding 0x12345678. This proves
alias composition, not an accidentally identity-mapped store. The entry
profile permits preserving the real RF flag on fault retry. Capability MSRs
advertise only four-level/WB/single-context INVEPT EPT support for this diagnostic,
not large pages, A/D, VPID or arbitrary nested operating systems.

`efi/tests/nested2.sh` runs sanitizer policy tests plus build and honest TCG
VMX-unavailable skip. Hardware success requires `NESTED.N2_PASS` accompanied by
`EPT12_PERMISSION_REFLECTED`, `L1_INVEPT_PHYSICAL_EPT02_FLUSHED` and
`EPT_ALIAS_PROVED`, not merely a compiled image.

## N2 hardware evidence (2026-11-09)

Isolated archbook QEMU/KVM, one vCPU, 256 MiB and 512 MiB both passed
`NESTED.N2_PASS EPT12_composed INVEPT_retry_alias_proved`.
ELF SHA256: `8e0b892dcbeb093a8bd47449b1b51b3c84c97150f719d8a110cd3d75a6e2e2e6`.
Both traces contain real L2 EPT violation (reason 48), EPT12 write-permission
reflection to L1, real L1 INVEPT exit (reason 50) and successful physical
EPT02 invalidation, L1 VMRESUME and subsequent L2 VMCALL. The backing check
proves L2 GPA f000 wrote 0x77 to L1 GPA 8000 while L1 GPA f000 was unchanged.
The original faulting store was retried, not skipped. All temporary QEMU
processes exited; no host settings or existing disks were changed.
Evidence: Library `libfile_c6a6f70634748191999992704de76e30`
(`nested-ept-evidence.tar.gz`). This validates only the bounded N2 profile
above, not arbitrary EPT topology, nested Hyper-V, or complete guest-state
virtualization.

Independent N2/N1/N0 sanitizer/build/TCG and all existing non-IOAPIC regressions
passed: `efi/build/nested2-review-full.log`, `FINAL_STATUS=0`.

## N3: supported guest-state snapshot and round trip (hardware pending)

The independent `vmx-nested-state` image exercises more than RIP/RSP/RFLAGS.
L2 natively executes LGDT/LIDT to set GDTR=5000:47 and IDTR=5900:15, sets
RSP=8800 and RFLAGS=403 (CF/DF, IF/TF clear), and writes three GPR sentinels.
L1 verifies those GPR values immediately after the reflected exit, VMREADs
RSP/flags/table state, then VMWRITEs the original descriptor table contract,
RSP=8700, RFLAGS=46 and the next L2 RIP. L2 uses SGDT/SIDT and native stack/flag
reads to verify the restored state, writes a second GPR sentinel set and exits
again. This is descriptor-table state testing, not proof of arbitrary segment
cache/CR/MSR virtualization. IF remains zero and no external event is injected.

N3 captures every supported guest-state field in the bootstrap metadata into
a temporary snapshot before atomically committing it to VMCS12. A malformed
snapshot cannot partially change the model. The VMCS link pointer is a fixed
control contract, not sampled guest architectural state. CR0/CR4/CR3 and segment
fields are captured but input values remain strict-profile: arbitrary CR
mask/shadow behavior and segment transformations are not newly supported.
GPRs remain actual L2 exit values outside VMCS; no old L1 register image is
restored. Physical VMCS02 host state remains L0-owned.

Conditional save/load fields are explicitly gated against actual physical
controls: exit save-debug (bit 2), save-PAT (18), save-EFER (20), and entry
load-debug (2), load-PAT (14), load-EFER (15) must all be set. Only with those
conditions can the corresponding VMREAD guest fields be treated as current.
Descriptor-table exiting must be off so LGDT/LIDT/SGDT/SIDT execute natively.
Unsupported configurations refuse the diagnostic rather than silently reporting
stale VMCS values as real capture. The existing MSR exits and strict profile
still prevent arbitrary unsupported state changes.

Only the supported VMCALL exit packet is reflected in N3; exception/error-code,
interrupt/vectoring and failed-entry reflection are not generalized. Physical
entry failure still stops and cannot be relabeled as L1 VMfailValid 7/8.
Unit tests cover failed snapshot atomicity, prohibited host/control writes,
conditional save-control absence, mutable-profile limits and TF rejection.
`efi/tests/nested3.sh` runs sanitizer checks/build/TCG skip. Actual proof requires
`NESTED.N3_PASS GPR_RSP_RFLAGS_GDTR_IDTR_roundtrip=2` on hardware plus both real
L2 exits and snapshot-field logs. N1/N2 retain independent images and tests.

## N3 hardware evidence (2026-11-09)

Isolated archbook KVM, one vCPU, 256 MiB and 512 MiB both passed
`NESTED.N3_PASS GPR_RSP_RFLAGS_GDTR_IDTR_roundtrip=2`.
ELF SHA256: `eb22f974497196c66e5ed70ce8b350e5e419e279348042521099b91ef035e547`.
Real L2 LGDT/LIDT and GPR/RSP/RFLAGS changes were reflected through a
52-field supported-state snapshot. L1 VMREAD checked the values and VMWRITE
prepared different state; real VMRESUME then let L2 verify the second state.
This is not arbitrary segment/CR/MSR-state support: the profile still restricts
those values, and virtual conditional MSR-save behavior is not generalized.
Evidence: Library `libfile_f9b6e5abfc70819198ed626c579679fb`
(`nested-state-evidence.tar.gz`). All temporary QEMU processes exited.

N3 independent full non-IOAPIC regression passed:
`efi/build/nested3-review-full.log`, `FINAL_STATUS=0`, including N0/N1/N2/N3
policies, all earlier VMX fixtures, boot/memory/GDB/display/console checks.

N3 review clarification: the save/load gates above describe physical VMCS02
capture, not virtual VMCS12 conditional-save semantics. VMCS12 virtual exit
controls in this profile contain only host-address-size bit 9; virtual
save-debug/PAT/EFER are not enabled. Those MSRs remain unchanged throughout this
payload, so copying their unchanged values has no observable difference here.
N3 does **not** verify modified MSRs or general virtual conditional saving.
Before permitting MSR changes, reflection must separately honor VMCS12's
virtual save controls; physical capture availability alone is insufficient.

## N4: early VMfail versus late failed-entry reflection (hardware pending)

The separate `vmx-nested-failure` payload performs actual L1 VMX instructions
for three distinct paths. With no current VMCS, VMREAD returns VMfailInvalid
(CF=1/ZF=0) without modifying its destination register. After loading VMCS12,
primary execution-control bit 0=1 is rejected as a proven virtual control
violation with VMfailValid error 7; virtual host CS=0 is similarly rejected
with error 8. Both fall through to the next L1 instruction, preserve guest
state/clear launch state, and do not change physical controls or L0 host state.
Only these exact early-field violations with all other profile values valid
are classified; other unsupported configurations still fail closed.

Then L1 sets guest RFLAGS.bit1=0, the sole intentionally invalid guest-state
value allowed into physical VMCS02 for this diagnostic. All physical host
fields remain L0-owned. The monitor accepts only a pre-armed actual physical
failed entry with reason 0x80000021 and qualification 0. Physical early VMfail
(stub return 0), an unexpected failed-entry reason or successful execution of
this invalid profile stops. It does not invent a corresponding L1 error.

The late-failure branch runs before ordinary guest-state reads/snapshot and
before setting physical `launched02`. It only updates VMCS12 exit reason and
qualification, cancels the prepared transaction, preserves prior launch state,
all guest-state fields, old instruction-error and other exit information, then
loads the validated virtual host into VMCS01 guest state. No fake successful
VM-instruction flags are applied. L1 verifies GPR sentinels, unchanged invalid
guest RFLAGS and prior error 8. VMRESUME must now fail with error 5, proving
that failed VMLAUNCH left the VMCS clear. L1 repairs RFLAGS to 2 and executes
VMLAUNCH again; one real L2 VMCALL completes the diagnostic.

Intel SDM Vol.3C 326019-084US §27's ordered entry sequence performs guest checks
and MSR loads before changing launch state. §27.3.1.4 requires RFLAGS.bit1=1.
§27.8 describes failed-entry reason bit31, ordinary invalid-guest reason 33,
qualification, lack of guest-state/MSR-store saves, and unchanged remaining
exit information; §28.5 loads host state (including host RIP/RSP and RFLAGS=2).
These rules differ materially from a normal VM exit. No generic failed-entry
handling, MSR-load failure, PDPTR failure or SMM behavior is claimed here.

`efi/tests/nested4.sh` runs pure failure-atomicity/profile/N0 regression tests,
builds the independent ESP and checks TCG's VMX-unavailable skip. The test suite
also proves a failed VMRESUME preserves an already-launched prior state, and
that unsupported failure packets leave the model unchanged. Hardware evidence
must include both early errors 7/8, `FAILED_ENTRY_REFLECTED` and the final
`NESTED.N4_PASS`, not just compilation.

## N4 hardware evidence (2026-11-09)

Isolated archbook KVM, one vCPU, 256 MiB and 512 MiB both passed
`NESTED.N4_PASS invalid_valid_failed_entry_distinct=1 corrected_launch=1`.
ELF SHA256: `91c21959ca01c40e59b86cdd9d65336931e2c5bda6897d9229d184a3d8759825`.
Real L1 VMX instruction tests separately verified no-current VMfailInvalid,
virtual early control/host VMfailValid errors 7/8, and the armed physical
VM-entry guest-state failure reason 0x80000021. The late failure reflects to
L1 without ordinary guest-state snapshot, preserving invalid guest RFLAGS,
old instruction error and clear launch state. L1 checks VMRESUME error 5,
repairs guest RFLAGS, then a new physical VMLAUNCH successfully runs L2.
This does not generalize all invalid-state/MSR-load/machine-check failures;
unexpected physical failures remain L0 errors and stop.
Evidence: Library `libfile_7d9ca5feca20819189c495856974b583`
(`nested-failure-evidence.tar.gz`). All temporary QEMU processes exited.

Independent N0–N4 and all existing non-IOAPIC regressions passed:
`efi/build/nested4-review-full.log`, `FINAL_STATUS=0`.

## Audit after N4: compatibility gaps and N5 rationale

N0–N4 do not constitute Hyper-V compatibility. Main missing work includes a
consistent broadly usable CPUID/VMX/MSR capability contract and instruction
exception ordering; arbitrary VMCS controls/state and virtual conditional MSR
saves; multiple VMCS/vCPUs and scheduling; nested EPT unmap/large-page/A-D and
concurrent invalidation; complete exception/event priority, vectoring and entry
injection; APIC/timers/device routing, timing and cross-vCPU synchronization.
Expanding fixture addresses alone would not close these architectural gaps.
N5 instead extracts an ownership policy usable across different exception RIPs.

## N5: exception ownership, not asynchronous interrupt virtualization

`nested_events` is a pure routing module independent of RIP, fixture phase and
handler address. Physical exception bitmap02 is L0-required bitmap OR bitmap12.
For the supported valid hardware #UD (type 3, vector 6, no error code), a bitmap12
hit belongs to L1 and is reflected, while an L0-only hit is reinjected into L2
at the original RIP. There is no assertion that every physical exit belongs to
L1. Unsupported vectors/types/error-code combinations, valid vectoring or entry
pending events, TF/pending debug and interruptibility blocking are rejected
without changing the proposed output. NMI/#PF mask-match/#DB arbitration remain
out of scope.

The independent `vmx-nested-events` guest first runs UD2 with bitmap12 bit6 clear.
L0 observes it but injects hardware #UD into the private L2 IDT6 handler, which
increments a counter, advances the saved fault RIP and IRETQs. Only the ordinary
subsequent VMCALL is reflected to L1. L1 enables bitmap12 bit6 and resumes to a
second UD2. This time L1 receives reason0 and interruption information and
chooses to advance guest RIP itself. The final VMCALL verifies the L2 handler
ran only once. Thus the two configurations establish different owners of the
same exception class without a fixed-RIP ownership rule. Fixture RIP checks
only verify the trusted diagnostic outcome.

Internal handling no longer pretends each physical exit is a new virtual entry:
N0 prepared state commits once on the first successful physical entry/exit;
L0 internal resume retains running state. Only a reflected exit returns the
virtual machine to L1. An outstanding internal injection must complete through
the trusted handler's normal VMCALL with no vectoring and exactly one handler
count before entry-valid is cleared. Any intervening unexpected exit fails
closed instead of risking duplicate injection. The monitor does not silently
consume an extra handler VMCALL as an internal protocol; the normal VMCALL
still reaches L1. Raw guest flags/RIP are preserved, including supported RF.

Exception reflection records valid exit interruption information and fault RIP;
it does not invent an instruction-length value for #UD. L1-requested VM-entry
injection is deliberately not implemented in N5: L1 demonstrates control by
skipping its owned UD2. This is not general asynchronous event arbitration.
`efi/tests/nested5.sh` tests pure ownership/negative cases, builds the independent
ESP and checks the honest TCG skip. Hardware success requires the internal
reinjection and delivery-confirmation markers, L1-reflection marker, and final
`NESTED.N5_PASS L0_reinject=1 L1_reflect=1 L2_handler=1`.

## N5 hardware evidence (2026-11-09)

Isolated archbook KVM, single vCPU, 256 MiB and 512 MiB both passed
`NESTED.N5_PASS L0_reinject=1 L1_reflect=1 L2_handler=1`.
ELF SHA256: `bb7ea56e956f81def2e0434cb971998770338feb0526f7c20d2e17c08897c83e`.
With L1 bitmap bit6 clear, a real L2 #UD was intercepted only for L0, injected
back at the original RIP, and handled once by L2's real IDT handler before
VMCALL. With L1 bit6 set, another real #UD was reflected to L1; L1 explicitly
skipped UD2 and resumed L2 to completion. Routing uses event metadata and
bitmaps, not a fault-RIP match. This does not implement L1-requested injection,
NMI, error-code/#PF routing, or interrupted event-delivery recovery.
Evidence: Library `libfile_b6b31fa3ea5c8191a5c9f539e4b2712c`
(`nested-events-evidence.tar.gz`). All temporary QEMU processes exited.

Independent N0–N5 and all existing non-IOAPIC regressions passed:
`efi/build/nested5-review-full.log`, `FINAL_STATUS=0`.

## N6: L1-requested #UD entry injection

Independent `make vmx-nested-inject`, `tests/nested6.sh`. This extends N5's
ownership policy, not the CPU feature contract to arbitrary events. The first
UD2 remains L0-only and is delivered internally. The second is intercepted by
L1, which uses actual VMWRITE for entry-info `0x80000306`, error=0, length=0,
then VMRESUME **without advancing fault RIP**. Bitmap12 bit6 remains set.
The L2 IDT6 handler runs again (r13=2), repairs its saved RIP and IRETQs. L1
reads entry-info after the final reflected VMCALL and checks exactly `0x306`.
There are two handler invocations total, one for each distinct fault.

Reusable `nested_entry_validate/arm/observe/release` separates entry-request
ownership from exception-interception ownership. Valid info must be hardware
exception type3/vector6/no error. Error and length zero are explicit profile
constraints, not claims that Intel consumes software length for #UD. Invalid
info residual bits are ignored. Blocking, TF, pending debug and valid vectoring
are rejected by this restricted profile; that does **not** mean Intel prohibits
all hardware exception injection with MOVSS. Existing L0 pending state and an
armed L1 event cannot coexist in this adapter. Uncertain delivery/intervening
exits fail closed; there is no automatic retry that could duplicate an event.

Normal virtual exit `nvmx_return_from_l2` clears only entry-info bit31, keeping
vector/type/etc. Physical L0-internal exits do not call this helper. Entry
commit alone does not clear virtual valid. Early VMfail and late failed-entry
preserve it. The physical entry fields are installed from the validated L1
request; only a trusted completed handler VMCALL confirms delivery in this
first diagnostic. The adapter never loads an L1 address into L0 host state.

References: Intel SDM 326019-084US, sections 25.8.3 and 28.2 (normal VM-exit
clears entry-valid), 27.8 (failed-entry exception to this clearing), 27.6.1.2
(injected event itself is not intercepted by VM-execution controls). Therefore
bitmap6 stays set while delivering the injected #UD. Fault delivery may itself
encounter a further exception/EPT fault; such combinations remain unsupported.
NMI, external interrupts, software exceptions, error-code exceptions, event
arbitration and recovery from partial delivery are not claimed here.

## N6 hardware evidence (2026-11-09)

Isolated archbook KVM, one vCPU, 256 MiB and 512 MiB passed
`NESTED.N6_PASS L0_reinject=1 L1_inject=1 L2_handler=2 entryvalid_cleared=1`.
ELF SHA256: `75eb6909df06f4a39bb3272944c14da30622c0441b112cd5422a82930159d124`.
After the second real #UD was reflected to L1, L1 requested VM-entry #UD
injection at the unchanged fault RIP. Bitmap12 bit6 remained set, but the
injected event was delivered to L2's handler rather than intercepted again.
The final normal virtual exit cleared only entry-info valid; real L1 VMREAD
verified 0x306 remained. L2 handler count was exactly two across L0 and L1
injection paths. VMfail/failed-entry retention is also covered by pure tests;
unknown interrupted-delivery outcomes still stop rather than retry blindly.
Evidence: Library `libfile_4b58f8c3792c8191853cc594e0bdf55b`
(`nested-inject-evidence.tar.gz`). All temporary QEMU processes exited.

Independent N0–N6 and all existing non-IOAPIC regressions passed:
`efi/build/nested6-review-full.log`, `FINAL_STATUS=0`.

## N7: nonzero error-code #GP routed through L1

Independent `make vmx-nested-gp-inject`, `tests/nested7.sh`. The first L0-only
UD path is retained. L1 then sets bitmap13. L2 loads DS with selector0x28;
the guest GDT limit remains **39 decimal (0x27)**, so this is a real #GP(0x28).
L0 reads actual exit interruption-info and, only if valid/error-valid are set,
actual exit-error-code (4406). The reusable route policy uses vector/bitmap,
not RIP; fixture assertions independently require this particular error/fault.
L1 VMREADs 4406 and forwards its value unchanged into entry-error-code4018,
writes info80000b0d and length0, then resumes without advancing the fault RIP.
Bitmap13 stays set. L2's private IDT13 handler checks stack error0x28, saved
fault RIP and original RSP9000, increments its invocation count, changes the
saved RIP to the labelled post-fault instruction, removes the 8-byte error
slot, then IRETQs. L1 verifies the final entry-info is0xb0d (valid cleared).

`nested_hardware_exception` is a reusable **supported subset** property check:
UD requires type3/vector6/no error-valid; GP requires type3/vector13/error-valid
and error bits31:16 zero. All 65536 low error values are accepted in pure tests;
selector EXT/IDT/TI are not blanket rejected. Only the concrete fixture requires
0x28. Other hardware vectors are unsupported, not declared architecturally
invalid. Malformed/reserved info, mismatched error-valid, nonzero high error
bits, conflicting pending ownership and uncertain delivery fail closed.

Intel SDM326019-084US section27.2.1.3 requires error-code bits31:16 zero when
error delivery is enabled, and reserves entry-info bits30:12. Our synthetic
VMX BASIC does not advertise bit56's relaxed error-code delivery behavior.
Hardware exception length is not a software-event length; profile uses zero.
Invalid entry-info/error residual fields are ignored by the event validator.
Normal virtual exit clears only valid, leaving error0x28 and lowinfo0xb0d;
failed-entry preserves both nonzero fields, verified by pure model tests.

No #PF/CR2, NMI, asynchronous arbitration, partial-delivery recovery, arbitrary
segment state or full Hyper-V compatibility is introduced by this milestone.

## N7 hardware evidence (2026-11-09)

Isolated archbook KVM, single vCPU, 256 MiB and 512 MiB both passed
`NESTED.N7_PASS actual_GP28_reflected_reinjected=1 frame_error_popped=1 entryvalid_cleared=1`.
ELF SHA256: `d21a21c3bae71a5cdd172311c4116be2fb9c51fd167b4748ee22c1ad26c9101b`.
A real L2 descriptor fault produced #GP error 0x28. L1 VMREAD retrieved the
actual exit error code and wrote it unchanged into the VM-entry request.
Injection used the original fault RIP; L2's handler checked the error and
frame, removed the eight-byte error slot, and returned with IRETQ. Normal
virtual exit cleared valid. The reusable GP validator accepts legal 16-bit
error codes; only the fixture asserts 0x28. Other unsupported exception types,
NMI, interrupted delivery and arbitration combinations remain rejected.
Evidence: Library `libfile_667deeb78a6c8191b61856eace811153`
(`nested-gp-inject-evidence.tar.gz`). All temporary QEMU processes exited.

Independent N0–N7 and all existing non-IOAPIC regressions passed:
`efi/build/nested7-review-full.log`, `FINAL_STATUS=0`.

## N8: nested #PF and the shared architectural CR2

Independent `make vmx-nested-pf-inject`, `tests/nested8.sh`. CR2 is NOT a VMCS
component. Virtual VM-entry/exit does not restore an old L1 CR2 or select a
separate L2 CR2 bank. The physical wrapper saves L0 CR2 and loads one shared
virtual-CPU CR2 slot for both VMCS01 and VMCS02. Every physical exit saves that
same architectural value and restores L0 before C; early VMfail also restores
L0. N8 sets a distinct nonzero L0 sentinel around every entry, immediately
reads and restores the caller's original CR2 before checks/logging/cleanup.
A physical failure remains fail-closed, with no speculative event retry.

L1 writes CR2=11110000 before the first launch. L2 later observes that value,
writes CR2=22220000 natively, and reads nonpresent GVAf000 (PTE[15]=8002).
All paging structures and the intended destination GPA8000 have valid EPT
backing. A direct #PF interception must NOT update CR2; the fault GLA is in
exit qualification. L0 reflects actual reason0/info80000b0e/error0 and that
qualification without assigning CR2. L1 verifies CR2 is still22220000, reads
qualificationf000 with VMREAD, writes CR2=f000 using real MOV CR2, and injects
the event through VM-entry fields. L2's gate14 checks CR2, error0, saved fault
RIP and original RSP; removes the error slot after advancing saved RIP and
IRETQs. On return L1 still observes f000, demonstrating shared state rather
than an invented VMCS host-CR2 restoration. Error0 is specifically this
supervisor read/nonpresent fixture, not a universal page-fault error.

Physical02 intercepts all PF with bitmap14=1, mask=match=0. The reusable pure
ownership rule is `bit14 == ((error & mask) == match)`; unlike other exceptions,
bitmap0 can also request an intercept when the comparison is unequal. Pure
exhaustive small-domain tests cover both bitmap values and mask/match cases.
The hardware profile currently permits only virtual mask=match=0. An internally
owned PF would require additional L0 CR2/event handling and is explicitly
rejected by the UD-only reinjection adapter; no fake UD-style PF delivery.
Reason48 is not treated as a page fault. Valid vectoring/indirect delivery
and complex faults remain rejected.

The supported PF error subset is bits0..3 (P,W/R,U/S,RSVD), not arbitrary GP
selector bits. Extended I/D, PK, shadow-stack and SGX cases are unsupported;
no corresponding CPU feature semantics are advertised by this synthetic
profile. This is stricter than Intel's structural entry-error high16-zero
rule, and unsupported values are not claimed architecturally illegal.

Intel SDM326019-084US: section28.1 (direct PF exit does not update CR2;
qualification contains fault GLA; indirect event delivery can differ),26.1.3
(CR2 MOV has no CR-access-exiting control),26.2 (PF bitmap/mask/match inverse
rule). Normal-exit valid clearing and failed-entry preservation remain as in
N6/N7. Unit tests verify the VMCS event fields; the CR2 assembly and native
MOV behavior require KVM evidence, not a fake pure variable test.

## N8 final hardware evidence (2026-11-09)

Final isolated archbook KVM, one vCPU, 256 MiB and 512 MiB both passed
`NESTED.N8_PASS PF_not_EPT=1 shared_CR2_L1_prepared=1 L0_CR2_restored=1`.
ELF SHA256: `9a2ce6a2ab9cc5ca7c38df245a49e2b6d7250db154e25ec5d9a700051c01d002`.
Real intercepted #PF left shared virtual CR2 at 0x22220000; the fault GLA
0xf000 came from exit qualification. L1 explicitly MOVed that GLA into CR2
before injecting #PF. L2's real handler verified CR2 and the exception frame.
Every physical L1/L2 return checked the independent L0 CR2 restoration.
L1/L2 share architectural CR2 across virtual VMX transitions; this is NOT
an invented per-VMCS CR2 switch. EPT violations are not treated as #PF.
The adapter supports mask/match 0/0 only and rejects L0-only PF reinjection;
the pure routing helper separately tests the full mask/match formula.
A first candidate passed hardware but printed a GP label for PF; only that
label was corrected, then the final hash above was retested on both sizes.
Full N0–N8/non-IOAPIC regression passed (`nested8-review-full.log`,
`FINAL_STATUS=0`); log-only correction passed `nested8-logfix-test.log`.
Evidence: Library `libfile_a959ed0eb9d481919fdda006083d4339`
(`nested-pf-inject-final-evidence.tar.gz`). All temporary QEMU processes exited.

## N9: real PF bitmap/mask/match routing, both owners

Independent `make vmx-nested-pf-route`, `tests/nested9.sh`. The adapter now reads
actual VMCS12 bitmap4004/mask4006/match4008 for each intercepted PF and passes
all three to the reusable ownership policy. Physical02 always has PF bitmap1
and mask=match=0, explicitly written on each requested entry, so L1's filter
cannot accidentally prevent L0 from observing faults. N9 permits arbitrary
32-bit virtual mask/match values; the diagnostic uses **mask2/match2** and
switches bitmap14 with real L1 VMWRITEs. Ownership is still the Intel equality
formula, not a fault label or expected stage.

The four hardware cases are deliberately ordered:

| Stage | bitmap14 | Actual access / PFEC | Comparison | Owner |
| --- | --- | --- | --- | --- |
| A | 1 | nonpresent read / 0 | unequal | L0 |
| B | 0 | nonpresent write / 2 | equal | L0 |
| C | 1 | nonpresent write / 2 | equal | L1 |
| D | 0 | nonpresent read / 0 | unequal | L1 |

Every stage writes a different original CR2 sentinel22220000+stage. A/B keep
the virtual CPU running across L0's physical exit; L0 sets shared guest CR2
from actual exit qualification and injects PF once, without generating a fake
L1 exception exit. C/D reflect the actual PF to L1 with CR2 still the sentinel;
L1 checks it, VMREADs qualification/error and uses native MOV CR2 before
requesting injection. All four native handlers check error/frame/RIP/RSP and
CR2, explicitly skip the fault, remove the error slot and IRETQ. Their normal
VMCALL checkpoints remain visible to L1 and clear virtual entry-valid normally.
There are four handler completions, two reflected exceptions and six virtual
exits total. Final intended GPA8000 backing stays0x42: skipped NP stores never
reach it. The PTE remains nonpresent throughout.

The lifecycle records pending origin L0 or L1 and never consumes/recommits a
virtual entry on an L0 internal retry. Any ambiguous intermediate delivery
exit stops, avoiding duplicate injection. Physical L0 CR2 retains the N8
per-entry sentinel verification and immediate original-value restoration.
Pure tests exercise the entire small-domain mask/match truth table (including
match bits outside mask), arbitrary 32-bit profile masks, malformed entry
info/error, pending conflicts and entry-failure preservation. This is still a
trusted single-vCPU direct-PF subset: no vectoring, NMI, asynchronous events,
page mapping repair, or expanded PF feature/error-bit contract.

## N9 hardware evidence (2026-11-09)

Isolated archbook KVM, one vCPU, 256 MiB and 512 MiB both passed
`NESTED.N9_PASS PF_four_way=4 L0_inject=2 L1_reflect_inject=2 shared_CR2=1`.
ELF SHA256: `6c2b82920eb953c39af9da553d9d956bd5776b4fa9b8b245794b22369222e5f9`.
Physical02 intercepts all #PF; routing reads actual virtual12 bitmap/mask/match.
With mask=2 and match=2, real PFEC 0/2/2/0 and bit14 1/0/1/0 exercised
L0/L0/L1/L1 ownership. L0-owned faults used qualification to set shared CR2
before internal reinjection. L1-owned faults preserved the old CR2 and were
reflected for L1 to prepare/reinject. All four real handlers verified their
frames and completed; their VMCALL checkpoints were reflected normally.
Routing is not based on a fixed RIP. This remains a bounded basic-PF profile;
unknown interrupted delivery, vectoring and advanced error-bit combinations
are not supported. No NMI or complete asynchronous arbitration is claimed.
Evidence: Library `libfile_8f3ef6f405cc8191848d33d5f1aab489`
(`nested-pf-route-evidence.tar.gz`). All temporary QEMU processes exited.

Independent N0–N9 and all existing non-IOAPIC regressions passed:
`efi/build/nested9-review-full.log`, `FINAL_STATUS=0`.

## N10: bounded multi-address composed-EPT map manager

Independent `make vmx-nested-ept-map`, `tests/nested10.sh`. N2's original
readable-pages1..15 manager remains available for its separate regression.
New `nested_ept_map` plans **all64 4-KiB pages, L2 GPA[0,0x40000)**. This is a
bounded address manager, not arbitrary-address nested EPT. It supports absent
EPT12 entries, unmap, remapping and permission reduction. All supported
present mappings are readable because W-without-R and execute-only remain
unsupported. Every EPT12 table/leaf GPA passes an L0-owned backing resolver;
no L1 address becomes an arbitrary HPA. The hardware resolver also checks
this immutable EPT01 fixture's three upper links and exact owned leaf mapping.

Pure planning validates the whole next image before any output publication.
The hardware manager writes every one of the64 leaves, including zero entries,
while the single vCPU is stopped; the other448 entries in its PT remain zero.
It then performs real single-context INVEPT. Only successful invalidation
commits the cached plan and increments generation. Flush failure poisons the
manager, retains potentially referenced pages and prevents re-entry or a false
L1 success. After VMCLEAR02, destruction requires successful invalidation
before freeing; persistent failure panics and retains the pages. Validation
failure (including missing L0 table/leaf backing) changes neither hardware
leaves nor committed plan/generation. Publication is a stopped-vCPU operation,
not a concurrent atomic multi-entry update claim.

Fault ownership uses the last **published** cached EPT12 permissions, not newly
edited L1 tables that have not undergone INVEPT. A present L1-RW/L0-RO write
failure is L0 backing denial, not a fabricated L1 violation. A true unmap is
L1 denial even when no leaf was resolved, with effective permissions zero.
Malformed tables and out-of-range accesses are unsupported; unknown faults
stop. Reflected qualification gets permissions from cached EPT12, not EPT02's
intersection with L0 restrictions. Direct data-access faults only are tested;
guest-page-walk-related faults are still rejected in this adapter.

Hardware sequence warms two nonidentity mappings with actual stores:
L2 GPA14000→L1 GPA8000 stores77; L2 GPA15000→L1 GPA9000 stores88.
L1 then makes the first mapping RO and removes the second, followed by actual
INVEPT. A write through the first old mapping must fault; L1 explicitly skips
that denied store. A read through the removed mapping must fault; L1 remaps
it to GPAa000 and issues a second INVEPT, then retries the same original RIP.
The read must return99 from new backing, not88 from the warmed old translation.
Final old data stays77/88; the revoked write did not reach backing. Three
published generations and exactly two requested INVEPT completions are checked.

ASan/UBSan tests include multi-alias plans, zero-permission ignored junk,
upper-level permission AND, boundary/high-address aliases, malformed tables,
L0 RO distinction, failure-atomic plans, all64-leaf unmap publication, partial
allocation cleanup, flush-failure poisoning and retained-page destruction.
The64-page limit, private RAM, no A/D/huge pages and single-vCPU scope remain
explicit; this is not yet arbitrary Hyper-V memory virtualization.

## N10 hardware evidence (2026-11-09)

Isolated archbook KVM, single vCPU, 256 MiB and 512 MiB both passed
`NESTED.N10_PASS warmed_multi_alias=2 revoked_write=1 unmapped_read=1 remap_new_backing=1 INVEPT=2`.
ELF SHA256: `3f613280c816627ddc4d67729d14a98e142c32d95f0e617e27a3c1f29cd962d7`.
Two nonidentity mappings were warmed by real L2 reads/writes. L1 changed one
to read-only and removed the other, then executed real INVEPT. The old write
and old read both produced actual EPT-violation exits. A second mapping update
and INVEPT remapped the missing address to new private backing; retrying the
original load returned 0x99. Three published generations and two L1-requested
physical invalidations were observed. Hardware tests exercise cache revocation;
L0-backing rejection and failed-flush poison/retention are pure/stub-tested.
The reusable manager covers 64 pages (256 KiB), 4 KiB WB, single context/vCPU;
out-of-range addresses are unsupported, not falsely reported as L1 faults.
Evidence: Library `libfile_529e4436c62881918b3728e9d4251842`
(`nested-ept-map-evidence.tar.gz`). All temporary QEMU processes exited.

Independent N0–N10 and all existing non-IOAPIC regressions passed:
`efi/build/nested10-review-full.log`, `FINAL_STATUS=0`.

## N11: sparse four-level EPT02 and internal miss fill

Independent `make vmx-nested-ept-sparse`, `tests/nested11.sh`. Stage A pure
`nested_ept_sparse` builds real four-level page tables with dynamically owned
nodes, not a larger GPA-indexed array. Table and metadata pages come from an
allocator with explicit unique-page ownership. Cached leaves have independent
metadata pages. A fixed architectural traversal depth is not an address-window
limit. GPA/HPA are checked against min(actual MAXPHYADDR,48); this hardware
fixture requires at least40 bits to exercise a512-GiB boundary. The wrapper's
256-page resource budget bounds memory use, not the representable GPA. OOM
leaves the existing published tree and epoch unchanged.

Missing paths are fully allocated offline before any link becomes visible.
After publishing ownership/links, real INVEPT must succeed before a fill is
usable. Failure poisons and retains every possibly referenced page; fill,
lookup and reset then reject. Lookup verifies the real owned table path/leaf
matches cached metadata rather than trusting or casting arbitrary HPAs. Root
initialization flushes its EPTP too, avoiding reliance on old allocation history.
The root remains stable across resets: clear all root entries, INVEPT, only
then release child/leaf metadata and increment epoch. Failed reset retains the
old ownership chain for safe destruction. After VMCLEAR02, destruction may
retry invalidation; failure retains pages rather than reusing cached memory.

Stage B starts with an empty composed tree. Actual EPT violations are looked
up by **guest-physical address2400**, not a possibly invalid GLA. On a cache
miss, the current EPT12 walk passes through private L0 backing; if both layers
permit the full nonzero access mask (bits0..2), fill EPT02 and resume the same RIP.
This includes internal guest-paging-structure reads/writes. The virtual entry
commits only once and stays running over these physical exits. Installed
leaves retain their original EPT12 snapshot until virtual INVEPT; permission
faults do not silently reread uninvalidated L1 edits. Nonpresent translations
are not negative-cached, so a never-installed miss may walk current EPT12.
Cached-permitted-but-still-faulting cases stop rather than entering a fill loop.

Only genuine L1 denials of direct final data accesses (qualification GLA-valid
and final-access bits) are reflected. L0 backing failures, malformed input,
indirect/vectoring faults and unsupported page-walk denials stop. Reflected
permissions come from EPT12, not the EPT02/L0 intersection. Read-walk derives
the complete permissions under the no-execute-only/W=>R profile; every bit of
a combined read/write/execute access mask must be permitted.

Hardware payload uses small low virtual addresses whose guest PTEs target
L2 GPAs f000,201000,40001000,8000001000. These cross PT, PD, PDPT and PML4
indices without allocating huge RAM; EPT12 maps them onto four private backing
pages. Actual stores warm11/22/33/44. L1 revokes write on the1-GiB branch and
removes the512-GiB branch, then INVEPT resets the entire sparse epoch. Reads
refill the surviving branches, revoked write and removed read must fault.
L1 restores the high branch to different backing, INVEPT resets again and the
original read retries to11 rather than old44. Four reflected virtual exits
and three epochs are checked separately from internal physical miss counts.

Pure sanitizer tests cover cross-level sparse addresses,32/48-bit bounds,
every offline path allocation failure, same-PT metadata OOM, stale-cache
classification, tampered paths, counter overflow, initialization/fill/reset
flush failures, ownership retention and safe destroy retries. N10's bounded
manager remains intact. This milestone still excludes huge pages, A/D, multiple
vCPUs/EPTPs and general Hyper-V compatibility.

N11 hardware bring-up correction: the initial sparse candidate was rejected by
its software entry profile after the warmed-alias CMP checks left arithmetic
RFLAGS (0x46, potentially 0x10046 with fault RF). This was not a hardware
VM-entry failure. Sparse entries now preserve and validate CF/PF/AF/ZF/SF/OF
and RF, require reserved bit 1, and continue to reject TF, IF, DF, IOPL and all
other unsupported bits. Entry RIP/RFLAGS and profile rejection status are
logged; no flags are normalized to fit the fixture. Pure tests cover both
accepted arithmetic combinations and rejected control/reserved flags. Cloud
TCG skips do not establish N11 hardware success; the corrected image must be
retested on KVM.

## N11 final hardware evidence (2026-11-09)

Final isolated archbook KVM, one vCPU, 256 MiB and 512 MiB passed
`NESTED.N11_PASS sparse_four_levels=1 internal_fills=0x17`.
ELF SHA256: `455a07faa740cc922f04facc1d8965e8048117c92ecd441bac838bc4a47d8b55`.
Starting with an empty EPT02 root, real paging-walk and data misses populated
23 mappings across three epochs without extra virtual L1 exits. Aliases at
0xf000, 0x201000, 0x40001000 and 0x8000001000 exercised distinct EPT levels.
After warming them, real INVEPT reset the epoch; revoked write permission and
an unmapped high-GPA branch produced L1 violations. A second reset/remap let
the original high-GPA read retry and read 0x11 from new private backing.
Nodes and metadata are dynamically allocated, not indexed by a fixed GPA
array. OOM at allocation points, unchanged published memory on failure, and
INVEPT poison/retain/retry cleanup are sanitizer/fault-injection tests, not
claims of hardware-forced OOM. Unknown indirect L1 faults remain unsupported.
An earlier hardware candidate failed because a profile rejected legitimate
CMP arithmetic flags plus RF (0x10046). The final profile preserves those
flags while still rejecting TF/IF/DF/IOPL and other disallowed state; both
final hardware runs observed 0x10046 and completed. Earlier failed evidence:
Library `libfile_9904786f638081919eefb0c9daf6d5c8`.
Final evidence: Library `libfile_49c58fd28cd8819189babc8a95b44689`
(`nested-ept-sparse-final-evidence.tar.gz`). All temporary QEMU processes exited.

Final corrected-image independent N0–N11/non-IOAPIC full regression passed:
`efi/build/nested11-flags-review-full.log`, `FINAL_STATUS=0`.

## N12: multiple sparse EPT contexts (restricted diagnostic)

`nested_ept_contexts` is a reusable, single-vCPU registry of up to four
simultaneously owned sparse EPT02 trees. This is a resource budget, not a GPA
window. Each complete valid EPTP12 has independent cached leaf metadata,
physical EPTP02, and generation. Selecting an existing context neither flushes
nor rereads its installed translations. New-context allocation is committed
only after initialization succeeds; OOM leaves the old selection unchanged.
A retained root after an initial flush failure is owned and poisons the whole
registry, never leaked as an uncounted allocation.

Intel SDM 326019-084US, Vol.3C §31.3 INVEPT: single-context invalidation matches
EPTP bits 51:12, not the complete descriptor. The registry compares that root
identity across all cached slots. Our advertised profile supports only WB,
four-level, A/D-disabled EPTP low bits 0x1e, so valid configuration aliases
of one root do not currently arise. A valid uncached root is a successful
no-op without requiring its guest page-table backing to exist. Type 2 reads
the guest m128 operand but ignores its low qword, including an invalid EPTP;
the high qword is restricted to zero (otherwise unsupported, not a fabricated
architectural error). Virtual type 2 is implemented by physical type-1 INVEPT
on every owned EPT02, not by passing an untrusted descriptor to the host.
Only N12 advertises the virtual all-context capability.

Reset clears each stable physical root, invalidates it, then reclaims its
children. All-context reset is intentionally **not rollback-atomic**: if a
later context fails invalidation, earlier successful contexts can already
have advanced generations and freed safe pages. The registry is poisoned,
no virtual VMsucceed is returned and no guest reentry is permitted. Potentially
cached pages remain owned until cleanup succeeds. Cleanup VMCLEARs VMCS02
before destroying all context trees. No context eviction or EPTP reuse occurs.

The independent fixture executes eight real reads at the same L2 instruction
site, with real L1 VMWRITEs selecting A/B EPTP12. Expected values are
11,22,11,22,33,22,44,33 (hex): two warmed mappings retain stale backing after
uninvalidated table writes; type 1 targeting A while B is current refreshes
only A; type 2 with descriptor low qword `dead` refreshes both. L0 decodes the
actual INVEPT instruction and operates on registry state, never identifies an
operation by fixture RIP. Checkpoint labels and values are test assertions,
not emulation. Physical EPT02 selection is rewritten before every virtual
entry. Only private L0-owned backing is resolvable; L1/EPT denial outside this
fixture's successful accesses stops rather than inventing a guest result.

Run `efi/tests/nested12.sh` for sanitizer pure tests, build and TCG skip checks.
Actual eight-checkpoint isolation/invalidation evidence still requires KVM;
a TCG skip is not a nested-hardware success.

### N12 real KVM evidence (2026-11-09)

Isolated archbook KVM with one vCPU, 256 MiB and 512 MiB both passed
`NESTED.N12_PASS isolated_contexts=2 type1=1 type2=1 stale_preserved=1`.
ELF SHA256: `b13f0ff90b0b503d602f505fbfe5e84c3472bdc9c785c04873017aa2aa6885eb`.
The actual eight read values were 0x11,0x22,0x11,0x22,0x33,0x22,0x44,0x33.
Both logs separately record successful virtual type-1 and type-2 invalidation.
All temporary QEMU processes exited; no host settings or existing VM disks
were changed. Evidence Library `libfile_f773004dc11c8191a735fc732f38a597`
(`nested-ept-context-evidence.tar.gz`). Context capacity/OOM and invalidation
failure retention remain pure fault-injection coverage, not hardware-forced
resource or instruction failures. The profile remains single-vCPU/4-KiB/WB,
not general Hyper-V or multi-vCPU support.

Independent N0–N12 and all previous non-IOAPIC full cloud regressions passed:
`efi/build/nested12-review-full.log`, `FINAL_STATUS=0`.

## N13: dynamic residency and safe eviction

The context registry now uses allocator-owned linked metadata pages rather
than a fixed four-entry array. `init_budget` chooses a resident-context budget
and whether inactive LRU eviction is allowed. The compatibility initializer
used by N12 keeps budget four with eviction disabled; N13 selects budget six
and runs eight distinct EPTP12 roots. The hardware allocator also retains its
explicit total-page budget of 256. These are resource limits, not GPA limits.

Each physical VMCS02 reference is pinned independently of software `active`.
To switch, L0 first selects/initializes the new context, pins it, VMWRITEs its
physical EPTP02, and only after success unpins the old reference. A failed
VMWRITE leaves both pins owned and stops. No VMCLEAR is used for ordinary
switches, so physical and virtual launch states are unchanged. Final cleanup
VMCLEARs VMCS02 before the explicit `release_all_after_clear` operation and
registry destruction. That operation is a trusted caller contract, not a way
to make live hardware references safe by merely changing a software counter.

Only non-active, unpinned contexts are eviction candidates. Metadata and the
new empty root are allocated and initialized offline first; only then is the
LRU victim invalidated and reclaimed. Thus allocation failure does not discard
an existing usable context or change active selection. This requires temporary
peak headroom and deliberately does not guarantee progress when the total
allocator budget is exhausted. If victim invalidation fails, both victim and
candidate remain owned, the registry is poisoned and no success/reentry is
allowed. LRU tick overflow is rejected rather than wrapping. Destruction with
any remaining pins is rejected without freeing resources.

The independent hardware fixture reads twelve values from eight EPTP roots,
with budget six: `11 22 11 22 11 22 11 22 33 22 44 44` (hex). The evicted first
context is changed in L1 then reselected, proving a fresh walk rather than a
stale alias; the still-resident eighth context retains its old mapping until
single-context INVEPT. All-context INVEPT then refreshes the first context.
Eviction-induced extra invalidation is allowed; no promise is made that
uninvalidated guest page-table changes remain invisible after eviction.
The private fixture expands to 47 owned pages for eight genuine four-level
L1 table trees; no extra host memory is exposed. `tests/nested13.sh` is the
pure/sanitizer/build/TCG-skip check; KVM is required for hardware proof.

### N13 final hardware and regression evidence (2026-11-09)

Isolated archbook KVM, one vCPU, 256 MiB and 512 MiB passed
`NESTED.N13_PASS eight_contexts=1 budget=6 safe_eviction=1 type1=1 type2=1`.
ELF SHA256: `ff3dc3c138036c1e9777117dda14c2ecf32810fc117eadfce7ee9e365a7a7f99`.
Eight distinct EPTP12 roots were selected with a six-resident-context budget;
the twelve actual values were 11,22,11,22,11,22,11,22,33,22,44,44 (hex).
Returning to an evicted root rewalked changed EPT12 backing, while a retained
root preserved its cached translation until the corresponding INVEPT.
Both type-1 and type-2 invalidation completed. All test QEMU processes exited.
Evidence Library `libfile_9688d0adb47481918f53165bfacb89f9`
(`nested-ept-dynamic-evidence.tar.gz`). Pure sanitizer tests cover OOM and
invalidation failure paths; those failures were not induced on hardware.
Independent complete N0-N13 and prior non-IOAPIC cloud regression passed:
`efi/build/nested13-review-full.log`, `FINAL_STATUS=0`.
This is resource-bounded dynamic lifecycle, not unlimited contexts, SMP, or
complete nested Hyper-V support. Existing one-profile EPTP low bits remain.

## N14: EPT12 large leaves, 4-KiB EPT02 composition

The explicit `nested_ept_policy` adds MAXPHYADDR and separate 2-MiB/1-GiB
leaf capabilities. The original walker is a strict 48-bit, 4-KiB-only wrapper,
so existing profiles do not silently gain new advertised features. Only N14
advertises virtual EPT capability bits 16 and 17. Physical EPT02 remains
four-level with 4-KiB leaves; physical large-page support is not required.

Intel SDM 326019-084US §29.3.2 Tables 29-3 and 29-5: PS is supported only in
PDPT (1 GiB) and PD (2 MiB); PML4/PT bit 7 is not a large-leaf selector.
Large-leaf address bits 29:12 or 20:12 respectively must be zero. WB memory
type remains bits 5:3; there is no ordinary guest-page-table PAT-bit relocation.
IPAT is architecturally legal but unsupported here. A/D fields are likewise
unsupported by this profile (some are ignored, not reserved, when A/D is off).
Present entries outside the declared profile fail closed; nonpresent RWX=0
is classified before inspecting the ignored remaining fields.

Effective permissions are intersected at every level. The walker computes
`large_base + (L2_GPA & (leaf_size - 1))`, validates physical width, and resolves
only that address's 4-KiB L1 backing page through the private L0 resolver.
It neither requires nor exposes a physically contiguous 2-MiB/1-GiB host
allocation. Nonzero page offsets and HPA offsets are preserved. Missing L0
backing remains a distinct fatal classification, never a fabricated L1 fault.

The hardware fixture combines normal 4-KiB EPT12 entries with a 2-MiB leaf
covering GPA 0x208000 and a 1-GiB leaf covering GPA 0x40009000. Both large
bases are zero and touch only private L1 pages 0x8000/0x9000. Warmed writes
store 0x11/0x22, then L1 revokes write permission on the 2-MiB leaf and removes
the 1-GiB leaf followed by real INVEPT. The resulting direct write/read faults
are reflected with EPT12-derived permissions; L1 skips the denied store,
restores the 1-GiB leaf, invalidates, and retries the original read to obtain
0x22. No arbitrary host address or entire large backing range is exposed.
Run `tests/nested14.sh`; pure/build/TCG results are not hardware proof.

### N14 final hardware and regression evidence (2026-11-09)

Archbook isolated KVM with one vCPU, 256 MiB and 512 MiB both passed
`NESTED.N14_PASS EPT12_2MiB_1GiB=1 EPT02_4KiB=1 revoked_and_unmapped=1`.
ELF SHA256: `329a68c263dc533cd6289cfc1dc64bb18239c4de75da215929b6066e73ca1f9a`.
Real L2 execution touched offsets within both EPT12 large-leaf sizes, using
small private backing pages and 4-KiB EPT02 leaves. After actual INVEPT, the
2-MiB write downgrade and 1-GiB unmap reflected genuine EPT violations;
restoring the 1-GiB leaf let the original read retry and return 0x22.
This does not claim physical EPT02 large pages or an entire GiB of backing RAM.
All temporary QEMU processes exited. Evidence Library
`libfile_5ad22bc2fd448191b5125314d2656af2` (`nested-ept-large-evidence.tar.gz`).
Independent N0-N14 and all previous non-IOAPIC cloud regressions passed:
`efi/build/nested14-review-full.log`, `FINAL_STATUS=0`.

## N15: restricted EPT12 A/D tracking

Intel SDM 326019-084US §29.3.5 specifies A on used EPT entries and leaf D for
writes. Critically, with EPT A/D enabled, guest paging-structure accesses are
write accesses even for paging reads (the PAE PDPTE-loading exception is outside
this IA32e profile). N15 therefore capability-gates physical EPT A/D (bit 21)
and enables AD in EPT02 as well as advertising virtual EPT12 AD. This gives
physical exits the correct access semantics, including combined read/write
qualification. It does not guess paging roles from fixed GPA addresses.

EPT12 writeback remains explicit software work, not a claim that hardware
updates two translation layers. A successful policy walk returns a trace of
each source entry's L1 GPA, raw identity, owned table HPA, and leaf role.
`nested_ept_ad_update` resolves all source pages through a separate mutable
private-RAM interface, checks R+W backing, same HPA and non-A/D identity, and
only after all checks ORs A into each entry and D into the leaf for writes.
It never casts away const from an untrusted pointer. Mapping changes without
invalidation are rejected rather than overwriting a new entry. This relies on
single-vCPU serialized private RAM, not a cross-core CAS or DMA protocol.

For a read-filled leaf with D=0 and genuine writable permissions, EPT02 is
installed temporarily read-only. Cached real permissions and installed
tracking permissions are distinct. A first write causes an internal L0 exit,
validates the saved source trace, updates D, then restores EPT02 W and performs
real INVEPT before retrying the same RIP. Failure stops; a flush failure poisons
and retains all owned pages. Physical EPT02 hardware A/D mutations are accepted
only in their proper places (A on nonleaf, A/D on leaf). Address, permission,
memory-type changes and nonleaf D still fail identity checks. Old AD0 profiles
retain their original strict checks.

The test uses one fixed AD-enabled EPTP12 (0x1005e), avoiding unimplemented
same-root AD0/AD1 transitions. Four real L2 checkpoints read 0x11, write 0x77,
read 0x77 after L1 clears A/D and executes INVEPT, then write 0x88. L1 and L0
check all traversed A bits, data-leaf D=0/1/0/1, and D on the EPT leaf covering
guest paging structures. The data translation is a 2-MiB EPT12 leaf; pure
traces also cover 4-KiB and 1-GiB leaves. Clearing bits without INVEPT is not
promised to restart tracking. This is not a complete dirty-log service.

Failed walks/permission faults can have architectural A side effects. This
first AD diagnostic stops on those cases rather than reflecting a supposedly
complete failure to L1. It only claims the validated successful path; previous
successful A/D updates are not rolled back on later failure. Unknown vectoring,
concurrent table changes, writeback-denied L0 backing, and unsupported states
fail closed. `tests/nested15.sh` covers pure identity/permission failures,
tracking promotion/flush failures, old strict behavior, build and TCG skip;
actual A/D behavior still needs KVM evidence.

### N15 final hardware and regression evidence (2026-11-09)

Archbook isolated one-vCPU KVM, 256 MiB and 512 MiB both passed
`NESTED.N15_PASS A_all_levels=1 first_write_D=1 clear_INVEPT_retrack=1 paging_D=1`.
ELF SHA256: `e4858406e9b642d291448b0cfde0176f78fc0f226a8b14754d77f58fdd87475f`.
Real EPT12 large leaf values changed 0x1b3 -> 0x3b3 on the first write.
After L1 cleared A/D and executed INVEPT, a new read produced 0x1b3 and a
new write produced 0x3b3 again. Paging-structure accesses were observed with
combined read/write mask 3 and EPT12 paging leaf 0x4333 (A and D set), matching
physical EPT02 AD-enabled semantics rather than a fixed-page heuristic.
Evidence Library `libfile_2d4c3af104348191a9d0095f0424ba3e`
(`nested-ept-ad-evidence.tar.gz`). All temporary QEMU processes exited.
Independent N0-N15 and all prior non-IOAPIC full cloud regressions passed:
`efi/build/nested15-review-full.log`, `FINAL_STATUS=0`.
This validates restricted successful translations and first-write tracking;
failed walks with partial A side effects remain unsupported and stop safely.
It is not a claim of complete dirty logging, multi-context AD, or SMP support.

## N16-A: allowed failed-walk A effects

The explicit `nested_ept_ad_walk_prefix` API retains the validated present
prefix for a nonpresent EPT12 terminal, or the full legal path for accumulated
permission denial. The previous successful-only API still clears its trace
on failure. Malformed/unsupported entries and L0 backing failures are not
reflected as a completed L1 violation. A-only prefix writeback prevalidates
all private mutable backing, HPA and entry identities before writing; a zero
prefix is a valid no-op. It sets no new D and never writes the nonpresent
terminal. Existing D bits in current memory are preserved, not rolled back.

This chooses a deterministic permitted software behavior based on Intel
SDM §29.3.5, §29.3.3.3 and §29.3.2. It does not claim to reproduce a particular
CPU's exact intermediate A-bit update order. An ancestor can already have A
from earlier code or paging translations; the test does not attribute such
shared bits exclusively to the failing access. L0 writeback failure stops and
does not falsely report completed L1-side effects.

The hardware fixture starts with a nonpresent data leaf whose exact sentinel
is 0xdead000. L2's read exits; validated ancestors have A, the terminal remains
unchanged, and L1 receives the reconstructed direct EPT violation. L1 installs
a read-only leaf and invalidates, then retries the same read successfully.
A subsequent cached write receives a genuine L1 permission violation with
leaf A=1/D=0 (distinct from an artificial L0 dirty-tracking write trap). L1
grants write, invalidates, and retries the original immediate store of 0x77.
Both repairs use real L1 INVEPT and no fault RIP is skipped. Other AD failure
classes still fail closed. Multi-context AD ownership remains the next
separate increment; this fixture alone does not complete that work.

### N16-A actual KVM evidence (2026-11-09)

Archbook isolated one-vCPU KVM, 256 MiB and 512 MiB passed
`NESTED.N16A_PASS nonpresent_prefix_A=1 terminal_unchanged=1 denied_write_D0=1 repaired_retry=1`.
ELF SHA256: `66b84a9db468dde38e2be3c00893b762f36416b7011e432ab2f8d616b64f844f`.
The nonpresent case updated only the present prefix A bits and left the
terminal unchanged; L1 repaired/invalidated and the original read retried.
A subsequent RO write reflected with A1/D0; after L1 repaired RW and INVEPT,
the original write retried successfully. This is an allowed deterministic
software implementation, not a claim to duplicate native CPU partial-update
microarchitectural ordering. Unknown malformed/vectoring cases still stop.
Evidence Library `libfile_3c7f04f39a4c8191ac0fae0ee389aa31`
(`nested-ept-ad-fail-evidence.tar.gz`). All temporary QEMU processes exited.

N16-A independent full cloud regression passed: `efi/build/nested16-review-full.log`, `FINAL_STATUS=0`.

## N16-B: context-owned A/D trace lifetimes

The dynamic registry now has an explicit initialization mode and lifecycle
hooks. Default initializers retain AD0 and low EPTP bits 0x1e. This independent
profile accepts only AD1 / 0x5e; it does not claim same-root AD0-to-AD1 switching.
Each context item owns an aligned opaque attachment containing its own A/D
cache header. Individual trace pages remain accounted allocator-owned pages.
Byte copies access the header without struct/opaque-array aliasing.

INIT initializes the header only after sparse-root initialization succeeds.
RESET releases that context's traces only after its root reset and real
invalidation succeed. DESTROY runs after successful tree invalidation and
before metadata release, including safe eviction and final cleanup. Failed
flush retains both tree and attachment; a partial all-context reset can have
released earlier successful items, but poisons the registry and returns no
virtual success. Hooks are trusted, non-failing and non-reentrant, not an
external plugin API. Pinning still protects actual VMCS02 references.

The fixture uses two distinct AD-enabled roots. A read leaves A's data D=0;
a write through B sets B's D while A remains unchanged. L1 clears A and uses
single-context INVEPT while B is current: A re-tracks from D=0 and B remains
correctly cached. A subsequent write sets A's D. Clearing both and using
all-context INVEPT rebuilds both traces, with two new reads leaving each data
D=0. Six returned values are 0x11, 0x77, 0x11, 0x88, 0x77, 0x88. A/D is still
shared L1 memory state if table pages are shared; writeback uses OR and never
rolls back another translation's bits. This fixture uses separate table trees.

Pure tests cover AD mode keys, lifecycle order/counts, failed initialization,
OOM, pinned eviction, victim flush failure, partial type-2 reset and destroy
retry. Hardware testing covers two resident AD contexts, not AD eviction under
load or cross-core synchronization. `tests/nested16b.sh` builds the independent
candidate and runs sanitizer/TCG checks; KVM evidence is required separately.

### N16-B final hardware and regression evidence (2026-11-09)

Isolated archbook one-vCPU KVM at 256 MiB and 512 MiB both passed
`NESTED.N16B_PASS context_owned_AD=1 type1_isolation=1 type2_retrack=1`.
ELF SHA256: `bdad414dd2a3b8b7a007af26bad6f7b36fbc53090a02efaf81ee8023c6d03aab`.
Actual A/B leaf state at successive checkpoints was
8133/9033,8133/9333,8133/9333,8333/9333,8033/9133,8133/9133 (hex).
Type-1 reset preserved the other context's tracking; type-2 reset let both
contexts retrack independently. All temporary QEMU processes exited.
Evidence Library `libfile_a743e2b6639c8191a6bbf3817b0e8fe2`
(`nested-ept-ad-context-evidence.tar.gz`). Independent full cloud regression
passed: `efi/build/nested16b-review-full.log`, `FINAL_STATUS=0`.
This still uses fixed AD1 mode; same-root AD0/AD1 switching and SMP are not
covered by this result. Lifecycle failure injection is pure-test coverage.

## N16-C: explicit same-root A/D mode transitions

`tests/nested16c.sh` builds `vmx-nested-ept-ad-mode`. The mixed registry is
opt-in: full EPTP keys `root|0x1e` and `root|0x5e` retain separate owned trees
and attachments; physical EPT02 A/D mode follows each key. Type-1 INVEPT
matches the root address and resets both aliases, with the existing flush,
poison/retain and hook lifetime rules. The old fixed-mode profiles remain
unchanged. Selecting a mode does not automatically invalidate an existing
translation: L1 is responsible for issuing the architectural invalidation.

The new read-only AD0 walker explicitly ignores bits 8/9 at every EPT12
level and never writes A/D. It accepts bits inherited from AD1; old strict
walker entry points retain their prior restricted contract. AD0 physical
misses use ordinary sparse fill, not AD traces, and AD1 retains per-context
first-write tracking. Unsupported cached faults still stop safely.

The real L1 fixture uses one root and performs six checkpoints:
AD1 read 11 (A1/D0), AD1 write 77 (A1/D1), explicit INVEPT then AD0 read 77
(inherited bits unchanged), clear all table A/D plus INVEPT then AD0 write 88
(all A/D remain zero), explicit INVEPT then AD1 read 88 (A1/D0), AD1 write 99
(A1/D1). It checks guest-paging backing A/D in AD1 and scans all four EPT12
tables for zero A/D in the cleared AD0 phase. Three actual decoded INVEPTs
are required. This is a single-vCPU trusted private-RAM profile, not a claim
about stale-mode behavior without software invalidation or cross-core
shootdowns. Intel 326019-084US Vol.3C sections 29.3.5 and 29.4 describe A/D
and invalidation requirements; section 31.3 defines root-scoped INVEPT.

Pure tests cover full-key separation, per-tree modes, same-root alias reset,
partial invalidation failure, invalid EPTPs and ignored inherited bits without
memory mutation. Candidate build and TCG VMX-unavailable skip pass; hardware
acceptance is recorded separately by the test coordinator.

### N16-C final hardware and regression evidence (2026-11-09)

Archbook isolated one-vCPU KVM, 256 MiB and 512 MiB both passed
`NESTED.N16C_PASS same_root_modes=1 explicit_INVEPT=1 AD0_no_writeback=1 AD1_retrack=1`.
ELF SHA256: `83cf1d578eef46263572b53fe78a1e55f26664eaae91186049cdee0fc684135b`.
Same-root leaf states were 8133,8333,8333,8033,8133,8333 (hex): AD0 preserved
existing A/D, did not rewrite cleared bits, and AD1 resumed read/first-write
tracking after L1's explicit root-scoped INVEPT. Both key aliases were
invalidated. This does not guarantee mode switches without invalidation.
Evidence Library `libfile_7033104846a08191b177410d367421ce`
(`nested-ept-ad-mode-evidence.tar.gz`); all temporary QEMU processes exited.
Independent full regression passed: `efi/build/nested16c-review-full.log`,
`FINAL_STATUS=0`. AD0 rejection paths in this mixed profile still stop safely;
complex malformed/vectoring cases and SMP are not covered by this milestone.

## N17: memory-type semantics and safe WB/IPAT hardware subset

`tests/nested17.sh` / `vmx-nested-memory` separates three contracts:

* Pure encoding and effective-type model: EPTP UC/WB controls EPT-table
  accesses; EPT leaves admit UC/WC/WT/WP/WB, PAT additionally admits UC-minus.
  CD overrides translated accesses to UC, IPAT bypasses PAT, otherwise the
  full Intel combination matrix applies (not a minimum-cacheability rule).
  See Intel Vol.3A table 11-7 (12-7 in newer editions) and Vol.3C
  326019-084US section 29.3.7.2. All 30 matrix pairs and malformed encodings
  are unit tested. NW/CD are additionally constrained by the hardware profile.
* Explicit walker metadata carries walk type, leaf type and IPAT through
  sparse cached results. Legacy strict WB APIs remain unchanged. Legal but
  unsafe types are not architectural misconfigurations: this diagnostic
  stops rather than installing them or forging an L1 violation.
* Physical admission currently requires WB EPTP, every leaf WB/IPAT=1 and
  CR0.CD=NW=0. This includes code, stacks and guest paging pages, not just
  data. Physical owned backing remains ordinary WB, so there are no UC/WC
  aliases of L0's WB direct map. Sparse validation and tamper checks preserve
  explicit IPAT metadata. The pure safety helper also models IPAT=0 with
  all eight PAT entries WB, but this additional path is not a hardware claim.

The trusted L2 has PAT `0606060606060006`: index0 is WB, index1 is UC.
GVA30000 and GVA31000 select those different entries while mapping the same
GPAf000/backing8000. Real stores/loads through both aliases produce 99 with
WB/IPAT leaves. L1 uses real VMWRITE for guest PAT and enabled virtual PAT
load/store controls, then VMREAD verifies the saved value. Physical entry
load-PAT, exit save-PAT and exit load-host-PAT are checked before running.
L0 host PAT remains its trusted template; virtual host PAT is restored into
L1's guest state. MSR writes are still intercepted, not freely permitted.

No one-GVA PAT observation is used to cache a permanent effective type for
an entire GPA. Non-WB hardware support, arbitrary guest PAT mutation, CD/NW
changes, and EPTP UC on actual WB-owned table memory remain unsupported.
They require kernel cache/mapping ownership and alias management first.
This fixture validates behavior and metadata/control wiring, not a cache
latency measurement or a claim of all memory types tested on hardware.

### N17 final hardware and regression evidence (2026-11-09)

Archbook isolated one-vCPU KVM, 256 MiB and 512 MiB both passed
`NESTED.N17_PASS WB_IPAT=1 PAT_alias_WB_UC=1 hardware_nonWB=0`.
ELF SHA256: `4df7b63a31ebdbc1bfc9f393cb2ffcaa7203357248d835a859c30417c244424f`.
Physical guest PAT was 0x0606060606060006; two GVAs selected PAT.WB and PAT.UC
for the same GPA, but explicit WB/IPAT on all EPT leaves made actual accesses
WB. Write/read 0x99 succeeded and PAT exit save/virtual-host restore checks
passed. This is not non-WB physical execution: UC/WC/WT/WP remain modeled and
rejected by hardware admission until host cache-alias management exists.
Evidence Library `libfile_7db15339dd80819188df29b6b55d3562`
(`nested-memory-evidence.tar.gz`); all temporary QEMU processes exited.
Independent full regression passed: `efi/build/nested17-review-full.log`,
`FINAL_STATUS=0`. Intel's type matrix coverage is pure testing, not a claim
that every modeled type is safely installed into physical EPT02.

## N25: explicit EPT12 misconfiguration classification

The opt-in `nested_ept_walk_classified` distinguishes architectural
MISCONFIGURATION from legal-but-UNSUPPORTED profile inputs and L0 backing
failure. Legacy walker APIs retain their old contracts. Zero RWX wins before
all other entry checks. Reliable malformed cases include present W without R,
physical address reserved bits through bit51, nonleaf reserved low bits,
leaf memory type 2/3/7, and misaligned large-page address bits. PT bit7 is
ignored, not PS; AD0 bits8/9 are ignored. Legal non-WB/IPAT/execute-only or
unknown unsupported feature fields are not automatically labelled malformed.
This remains a narrowed feature profile, not the full MBEC/SPP/#VE decoder.

The sparse adapter classifies only an uninstalled EPT12 translation on an
actual physical EPT02 miss. A physical EPT02 misconfiguration is an L0 failure,
never reflected as L1's reason49. Cached metadata remains authoritative until
INVEPT; L0 private table/leaf backing errors stop rather than becoming reason49.

The fixture exercises real L2 loads at GPA f000/e000/208000 through respectively
W-without-R, invalid memory type2, and misaligned 2MiB EPT12 leaf. L1 receives
reason49 and GPA, repairs the corresponding entry, executes real emulated
INVEPT (including successful physical invalidation), and retries original RIP.
All repaired aliases read 0x42. Hardware EPT02 always remains valid 4KiB/WB.
Only reason/GPA and normal guest state are reflected for misconfiguration;
undefined qualification, GLA, length and interruption fields are not invented
from the physical reason48. Their retained software values carry no guarantee
and L1 does not consume them.

Evidence basis: Intel 326019-084US Vol3C §28.2.1 (GPA defined for EPT
misconfiguration), §29.3.3.1 and Tables29-2/4/6/7. PT bit7 and nonleaf reserved
bit meanings differ; the classifier deliberately does not share one PS mask
across all levels. Non-WB physical mappings and general reason49 variants
remain outside this bounded implementation.

### N25 final hardware evidence (2026-11-09)

Final archbook isolated KVM, one vCPU, 256 MiB and 512 MiB passed
`NESTED.N25_PASS EPT12_misconfig49=3 repaired_INVEPT_retry=3 L0_fault_not_reflected=1`.
ELF SHA256: `a59e1a4ed7a77f341ce1a903ab0786172197da4bcf63bcdd32e13fc7ada17b40`.
Real L2 accesses at GPAs f000/e000/208000 caused software classification of
L1's W-without-R, invalid leaf memory type 2, and misaligned 2-MiB leaf.
L1 observed virtual reason 49 and GPA, repaired each entry and executed real
INVEPT; the original instructions retried successfully. Only defined reason49
fields are reflected, not the physical reason48 qualification/GLA/length.
L0-backing non-reflection is code/pure-negative coverage, not a forced hardware
backing failure. Physical EPT02 stayed valid throughout.
The first candidate failed on its second INVEPT because a diagnostic guard
reused the older two-step N11 scenario. The corrected N25 three-step guard has
explicit negative tests; this is not an architectural INVEPT count limit.
Failed evidence: `libfile_d318d1b842ec8191b89f85c73839b1d8`.
Final evidence: `libfile_db757ad10ddc81918c1dd0a144ddcb40`
(`nested-ept-misconfig-final-evidence.tar.gz`). All test QEMU processes exited.
Legal unsupported/ignored combinations still stop conservatively; this is not
an exhaustive EPT misconfiguration classifier or vectoring-fault implementation.

N25 corrected-candidate independent full regression passed: `efi/build/nested25-fixed-review-full.log`, `FINAL_STATUS=0`. The earlier `nested25-review-full.log` belongs to the hardware-failed candidate and is not final evidence.

### N27 hardware evidence (2026-11-09)

Archbook isolated QEMU/KVM `-cpu host -smp 1`, 256 and 512 MiB both exit 33
with `NESTED.N27_GUEST_PASS`, `UC_LEASE_RESTORED_WB`, and `N27_PASS`.
Real L2 reads returned 0x77 then 0x99 across L1 INVEPT, with two GPA aliases.
The final markers establish cleared VMCSs and flushed EPT references before
host UC-to-WB restoration. No temporary QEMU remained running.
Kernel SHA-256: `59a6bedcc0b194ae2771720d30923fb8f83d9e55e2193ab19149d19d87532012`.
EFI SHA-256: `0415ffcc51b4422d945517d779bee7eb58cb7db52377c7cf29c609718a576761`.
Library evidence: `libfile_a2352b8bbfa08191bfce86f342916a79`.
This is registered single-CPU/single-context UC+IPAT backing, not arbitrary
non-WB EPT12 types, multi-context leases, or SMP cache-type transitions.

N27 final independent full regression: `efi/build/nested27-review-full.log`,
`FINAL_STATUS=0`; kernel and EFI hashes remained identical to tested artifacts.
This target uses the shared `stage0-smoke.so` loader, also currently matching
N17/N26 EFI artifacts; the difference from older recorded EFI hashes has not
been attributed without an old-binary comparison.

### N28 hardware evidence (2026-11-09)

Corrected kernel `bc77d4cd0fcb6751e57b22a594e6e02819ce6597fb1d5ea191966431633fb7ab`
passed isolated archbook QEMU/KVM `-cpu host -smp 1` at 256/512 MiB, exit 33.
Both logs show seven real UC reads 0x77 through 0x7d, type1/type2 INVEPT,
`N28_GUEST_PASS contexts=3 budget=2 eviction=1 ... refs=2`, followed by cleared
VMCSs/flushed EPTs and `N28_PASS all_contexts_destroyed=1 refs=0 restored_WB=1`.
No temporary QEMU remained. Evidence:
`libfile_7d2d3d53cd9c81918ec3f27ad99f8f38`.
The earlier 2d8edf candidate was rejected before hardware testing for C effective
storage type aliasing; the final token is an explicitly typed union member.
This tests multi-context lifetime for one registered UC backing on one CPU;
it does not validate SMP cache transitions or other non-WB types.

N28 final independent full regression: `efi/build/nested28-review-full.log`,
`FINAL_STATUS=0`, 55 marked tests plus console-memory checks, final kernel/EFI
identical to both hardware runs. Paused IOAPIC remains excluded/unverified.

### N29 hardware evidence (2026-11-09)

Kernel `aab704da912ff3ce86f12786db5cea579b1d49bc9ae098fb3ec07f4b0278df3f`
passed isolated archbook KVM `-cpu host -smp 1` at 256/512 MiB, exit 33.
Both runs read 0x77 then 0x99 via safe UC/IPAT0 aliases, retaining the cached
leaf without INVEPT. Changing an unused guest PAT slot to WC caused
`N29_EXPECTED_UNSAFE_PAT_REFUSAL cached_leaf=1 no_physical_entry=1` before
physical re-entry. Final `N29_PASS` confirms three safe aliases, unsafe-PAT
refusal and WB restoration after VMCS/EPT cleanup. No temporary QEMU remained.
Evidence Library: `libfile_e960fce647e88191bd3c8e35b70b3cd0`.
Earlier 4c314 candidate was not hardware tested: independent review found an
old diagnostic INVEPT gate incompatible with intentionally cached re-entry.
This is conservative whole-PAT admission, not arbitrary per-GVA type tracking.

N29 final independent full regression: `efi/build/nested29-review-full.log`,
`FINAL_STATUS=0`, 56 marked tests plus console-memory; final kernel unchanged.

### N30 hardware evidence (2026-11-09)

Kernel `0b802da4542d343145f9dc1b2ff7eca1df59d56f946c69a6c40def5dfac98c2e`
passed isolated archbook KVM `-cpu host -smp 1` at 256/512 MiB, exit 33.
Existing host PAT was `0x0007040600070406`; no PAT/MTRR rewrite was needed.
Logs verify real EPT02 type4/IPAT1 leaves at GPA f000/e000, two-alias reads
0x77 and 0x99 across explicit L1 INVEPT, and `N30_PASS` after EPT cleanup
and restoration to WB with PAT unchanged. The runtime MTRR-WB proof and
encoding/alias checks establish the type contract; successful reads alone do
not establish hardware cache policy. No temporary QEMU remained running.
Evidence Library: `libfile_fe153e935bc88191b933cc954a49e969`.
Scope is registered single-CPU WT/IPAT1 backing, not arbitrary PAT/WT mixes,
WC/WP support or SMP memory-type transitions.

N30 final independent full regression: `efi/build/nested30-review-full.log`,
`FINAL_STATUS=0`, 57 marked tests plus console-memory. Final kernel unchanged
from both hardware runs; paused IOAPIC remains excluded.

## N32A: explicit nested vCPU context (single CPU)

Two opt-in diagnostics, `make -C efi vmx-nested-context` and
`vmx-nested-context-pf`, migrate the N1 and N8 adapters to an explicit
`nested_vcpu`. The context owns the 15-GPR image, the architectural CR2 slot
shared by L1/L2, fixture, VMCS physical-owner identity and launch/clear state.
The per-call assembly wrapper stores its host stack anchor and original CR2
without shared scratch. Its complete frame ABI is checked with `offsetof`
assertions; it is not interchangeable with the SMP diagnostic frame.

Before each physical entry the adapter checks owner and VMPTRST, then writes
HOST_RSP/HOST_RIP from the current trusted call. VM exits and VMfail paths
save the guest frame and immediately restore host CR2. Status distinguishes
VMexit, VMfailInvalid, VMfailValid and host-field VMWRITE failure. This selected
profile stops on physical entry failure; it does not yet migrate N4 failed-entry
reflection. Host descriptor limits are restored before the complete local host
state comparison. R14/R15 sentinels survive both virtual levels and host CR2 is
separately tested. Virtual exit continues to preserve L2 GPRs, not restore a
pre-entry L1 snapshot. CR2 is not a VMCS field and virtual host loading does not
restore an old L1 CR2.

The context remains alive through physical VMCLEAR and VMXOFF; only then may
its fixture be freed. A failed clear retains resources by stopping. Owner,
launch/resume, busy, overflow and failure policies have independent sanitizer
tests. `efi/tests/nested32.sh` also verifies that selected images do not link
the old global GPR/CR2/entry symbols. TCG can only exercise capability refusal;
real N1/N8 success requires KVM evidence.

Scope: this is a single-CPU migration of two selected profiles, **not SMP
nested execution**. The remaining macro diagnostics retain their existing
entry wrapper. The virtual VMCS model/profile are still adapter-call-local,
not a scheduler-owned resumable domain; PMM, context registries, A/D writeback
and typed backing services have not thereby become concurrent-safe.

N32A candidate correction: the original context probe omitted VMX_ROOT_TEST,
so four KVM boots only printed ROOT_TEST_DISABLED and are not guest passes.
The corrected build enables root execution and adds a compile-time dependency
assertion. The short test checks the probe relocation to guest_run_context,
entry symbol presence and absence of ROOT_TEST_DISABLED in the final images,
independently of TCG's legitimate no-VMX skip. Corrected hardware evidence is
required; the original candidate hashes are not accepted.

### N32A hardware evidence (2026-11-09)

Initial c8d18/33b9ca candidates were NOT successful: missing VMX_ROOT_TEST
compiled ROOT_TEST_DISABLED; exit33/SMOKE_PASS was not counted as guest proof.
Failed evidence: `libfile_5eeffe88e7cc8191b9ae948dfcc58405`. Added compile-time
macro dependency and final-image/real-entry checks, then rebuilt.

Corrected N1 kernel `305c7aac22d60de03e4aa4061de1545539e47f95399b5c01e71266c6fe8ae8a3`
and N8 kernel `ca4ead6c224378d5d167527348f837dbc451243d3b452968dfe51527e720825f`
each passed isolated archbook KVM `-cpu host -smp 1`, 256/512 MiB (four runs).
N1 actually launched/resumed L2 with two VMCALL reflections; N8 reported true
PF/not-EPT, L1-prepared shared virtual CR2 and restored L0 CR2. All four logged
N32A_CONTEXT_PASS with two GPR sentinels/per-call host CR2/owner0. No test QEMU
remained. Final evidence: `libfile_c59a4a061c28819182557c1ac7e60adf`.
Scope is selected single-CPU context paths, not yet parallel nested vCPUs.

N32A corrected final independent regression: `efi/build/nested32-fixed-review-full.log`,
`FINAL_STATUS=0`, 60 marked tests plus console-memory. Both final hashes unchanged.
Old `nested32-review-full.log` is ABORTED_ROOT_DISABLED, not final evidence.

## N32B: two independently owned nested instances

`make -C efi smp-nested` / `efi/tests/nested32b.sh` selects only the basic N1
adapter plus the N32A context wrapper. This is **two independent L1 instances**,
not two vCPUs of one L1 VM and not concurrent shared EPT12 management.

BSP preallocates both VMCS01/02 pairs and private fixtures before starting AP.
`ept_fixture_prepare` does not execute INVEPT; each local runner flushes its own
EPTP after entering VMX. The AP runner cannot call the global PMM or console:
its selected adapter uses a reserved VMCS02 and a bounded private trace. Fatal
paths publish a fault and park without reclaiming live hardware resources.
The selected objects' undefined-symbol checks reject legacy PMM/console/entry
calls. All per-instance EPT and guest page tables, stacks, GDT and virtual VMCS
regions remain private. Only GPA a000 maps one shared WB data page; the original
owned backing arrays are not replaced, so borrowing cannot double-free it.

BSP releases AP's run request and enters its own nested instance without first
waiting for AP completion. Each actual L2 writes its own cache-line token,
waits for the peer token, publishes observation and waits for reciprocal
observation before its first VMCALL. Guest polling has a bounded RDTSC deadline;
unexpected exits/faults fail closed. R14/R15 and the architectural L1/L2-shared
CR2 slot carry distinct per-CPU sentinels. Owner-local VMCS handling and host
state restoration remain mandatory at every entry/exit. The handshake is a
trusted diagnostic protocol, not a general guest scheduling primitive.

Only after both CPUs have cleared their physical VMCSs, left VMX and the AP
has published permanent park does BSP read final traces and release owned
pages. Timeouts, faults or missing safe completion retain all resources. The
old per-fixture global allocation-counter assertion is retained for old tests;
the new owned-release API instead releases the explicitly owned arrays after
the caller's two-owner lifetime proof. No dynamic nested EPT registry, typed
lease, A/D writeback, shared VMCS12 or concurrent PMM is enabled by this target.
TCG validates refusal only; real two-CPU overlap requires separate KVM evidence.

### N32B verification status (2026-11-09)

Candidate kernel `35664b81d2906fdf740dc7ae94e7ab5138293c83462f517e6a15e930faca7c2c`
passed independent cloud regression `efi/build/nested32b-review-full.log`,
`FINAL_STATUS=0`, 61 marked tests plus console-memory. This includes pure tests,
builds and TCG capability rejection, NOT actual two-CPU nested overlap proof.
Archbook attach returned offline before any N32B KVM run. Candidate is frozen
pending hardware verification; packaged image Library ID:
`libfile_551261fcbb5c81919295357d8c7cc959`.

### N32B hardware blocker resolved (2026-11-09)

After archbook reconnected, frozen kernel
`35664b81d2906fdf740dc7ae94e7ab5138293c83462f517e6a15e930faca7c2c`
passed isolated QEMU/KVM `-cpu host -smp 2` at 256 and 512 MiB, both exit33.
Each log contains two N1 launch/resume proofs, two N32A context proofs with
owners0/1, and `SMP.N32B_PASS nested_CPUs=2 L2_mutual_barrier=1
private_VMCS01_VMCS02=2 owner_clear_off=2`. No failure/timeout/panic markers.
Both test processes ended, no QEMU remained. Hardware evidence Library:
`libfile_7285300a94548191bc3370e71333f8e8`.
Combined with the previously completed unchanged-candidate full regression,
this validates overlapping execution intervals of two independent nested L1/L2
instances. It does not prove shared-L1 SMP, physical host-core simultaneity,
or concurrent nested EPT12 mutation/AD synchronization.
