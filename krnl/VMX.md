# VT-x bring-up: read-only probe and opt-in diagnostics

Default boot only calls `vmx_probe()` after the kernel owns CR3. It reads CPUID,
then, if VMX is exposed, FEATURE_CONTROL, VMX_BASIC and CR0/CR4 fixed masks.
It reports a rejection reason or capability pass. It never enables or locks
firmware FEATURE_CONTROL policy, and never executes VMXON by default.

## Optional root and trusted guest diagnostics

`make vmx` builds the VMXON → VMXOFF diagnostic.
`make vmx-guest` builds a separate ESP with a minimal trusted 64-bit guest:

```
VMXON → initialize VMCS → VMPTRLD → VMLAUNCH
      → VMCALL #1 → dispatch → VMRESUME
      → VMCALL #2 → dispatch → VMRESUME
      → VMCALL #3 (normal end) → VMCLEAR → VMXOFF
```

The guest implementation uses the current host CR3, RX kernel payload code,
and a separate static guest stack. **This is not an isolated virtual machine.**
It must not run arbitrary/untrusted code. There is no EPT, general event loop,
interrupt forwarding, guest firmware, Windows, nested VMX, scheduler, SMP,
DMA or VT-d in this diagnostic. It is a bounded three-exit CPU bring-up test.

Control MSRs determine allowed/required bits; unsupported forced features reject.
The diagnostic requires PAT/EFER entry/exit support and initializes host/guest
segment, table, control-register, SYSENTER, debug and MSR state. The assembly
exit path restores the host stack and callee-saved registers. The expected exit
must match reason 18, the phase-specific VMCALL RIP, length 3, all 15 GPRs,
RSP and RFLAGS. Assembly saves guest GPRs before returning to C and reloads
them before VMRESUME; RSP/RIP/RFLAGS are VMCS-managed. The guest also checks
restored RFLAGS after each resume. Only accepted nonterminal VMCALLs advance
RIP by the checked length. Terminal phase 2 is not resumed. Unexpected exits,
invalid state and entry failures stop the bounded loop without RIP advancement.
Only all three validated exits print `VMX.GUEST_VMCALL_PASS` and
`VMX.GUEST_RESUME_LOOP_PASS exits=3 resumes=2`. VM-entry failures and unrelated
exits are failures, not successful guest runs.

VMXON and VMCS pages respect BASIC's physical-address restriction. Before
writing a region, `vmx_memory_region_wb()` calculates a conservative WB-only
answer from enabled fixed/variable/default MTRRs, physical-address width,
PAT slot 0, and CR0 cache flags. Allocator admission and kernel mapping must
also provide ordinary WB RAM with PAT index 0. Unsupported/ambiguous states
reject; no MTRR/PAT writes are performed. The check is bootstrap/single-CPU
only and assumes no concurrent cache-policy mutation. It does not replace
hardware validation of the read-MSR path.

Failed VMXON restores CR4 and frees the unused region. Failed VMXOFF or final
VMCLEAR panics without freeing potentially live structures. Unexpected CPU
faults use the existing panic handler; no exception-recovery mechanism exists.

## Verification

```
source syle/efi/scripts/cloud-env.sh
cd syle/efi
tests/vmx.sh
tests/vmx_guest.sh
```

Current cloud: no `/dev/kvm`, no exposed VMX. QEMU TCG cannot execute Intel
VMX. The tests verify synthetic control/memory-type policies with ASan/UBSan,
compilation/linking, and the safe VMX-unavailable return path. The cloud alone does not validate VMXON, hardware MTRR reads, VMCS state
validity, VMLAUNCH or actual VM exit; see the separate KVM result below.
A successful generic smoke/debug-exit code is NOT VMX success; unavailable,
blocked and failed VMX paths can continue to the generic smoke marker.

On a separately authorized Intel/nested-VMX environment, use the diagnostic ESP
`build/vmx-guest-esp`, 1 vCPU and a bounded run. Require both
`VMX.GUEST_RESUME_LOOP_PASS exits=3 resumes=2` and
`VMX.ROOT_ROUNDTRIP_PASS`; reject missing, blocked,
skipped, panic or failed markers. The regular run.sh intentionally remains
TCG, not a pretend hardware-validation harness. No host configuration or
network restrictions were changed to obtain test access.

## Nested-KVM execution verification (2026-11-08)

The diagnostic was run twice on the authorized archbook (Intel Core Ultra 9
185H, QEMU 11.1.3/KVM, `-cpu host`, one vCPU), with independent 256 MiB and
512 MiB runs. Both observed `VMX.ROOT_ENTERED`, exit reason 18 (VMCALL),
`VMX.GUEST_VMCALL_PASS` and `VMX.ROOT_ROUNDTRIP_PASS`. This is execution of
Syle's trusted one-shot guest under nested KVM, **not** validation of nested
Hyper-V, Windows boot, EPT isolation or a production hypervisor.

The first run exposed a stale firmware FS selector (`0x30`) after replacing
the GDT. Kernel GDT setup now clears FS, GS and LDTR; invalid segment rejection
also logs its cause rather than returning silently. Explicit virtio-blk
`bootindex=1` was used after intermittent OVMF SATA boot stalls.

The captured test evidence is retained in Library item
`libfile_17ed7b5312748191bd03c679c286b927`. Host boot settings and existing
virtual machines were not changed. Cloud TCG regression remains a separate
check of the VMX-unavailable branch, not additional hardware execution.

## Resume-loop implementation

The earlier KVM evidence above covers the original one-shot test only. The
new bounded resume implementation has passed synthetic dispatch tests (including
wrong phase/reason/RIP/length/RSP/RFLAGS and corruption of each saved GPR)
and TCG safe-skip verification. The updated loop was then run twice on the
same authorized archbook QEMU/KVM environment, using independent 256 MiB and
512 MiB guests. Both runs verified three VMCALL exits and two VMRESUME entries,
per-exit guest-state checks, normal terminal dispatch and the VMXOFF round trip.
The loop evidence is retained as `resume-evidence.tar.gz`, Library item
`libfile_c06148fcc7a88191a51b9088f1fda61d`.
This is still a trusted shared-page-table diagnostic, not EPT isolation or
nested Hyper-V validation.
The diagnostic does not preserve or virtualize general SIMD, interrupt or
per-vCPU state beyond its deliberately restricted integer-only payload.
