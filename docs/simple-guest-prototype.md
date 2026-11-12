# Independent guest-image technical prototype

This opt-in prototype separates a guest executable from the hypervisor and
implements its loading, initial state, bounded execution and cleanup path.
It does **not** select the final guest OS, firmware, storage model or boot
protocol for milestone 2. The supplied guest is a small integer-only assembly
program, not a lightweight operating system or a completed milestone.

## Build and host verification

From the source root, with GCC/binutils and GNU-EFI available:

```sh
# This workspace only; elsewhere set EFI_PREFIX for your installed GNU-EFI.
. efi/scripts/cloud-env.sh
make -C efi simple-guest
bash efi/tests/simple_guest_host.sh
```

The host test command builds both the normal and opt-in images. It does not
start QEMU, firmware, KVM or a guest. Existing `make all` remains unchanged.

The separate ESP is `efi/build/simple-guest-esp`:

```text
EFI/BOOT/BOOTX64.EFI
syle/boot.cfg
syle/krnl.elf
syle/guest.elf
```

`guest.elf` is independently compiled from `guest/simple/main.S` and its linker
script. It is not linked into the kernel, and the new runner never enters the
older embedded VMX diagnostic payload. Replacing an admitted image does not
require changing fixed VMCALL RIPs or a diagnostic phase table in the kernel.

## Provisional loading and memory contract

Only the `SIMPLE_GUEST_PROTOTYPE` build reads `syle/guest.elf` before
ExitBootServices. It temporarily transports those bytes through the existing
V4 `user_image` / `user_image_buffer` ranges instead of a management image.
The record layout and normal management path are unchanged. This reuse is
private to the opt-in build, not a new stable ABI or a claim that Ring 3 is a VM.

The kernel validates a little-endian x86-64 ET_EXEC ELF with at most eight
page-disjoint PT_LOAD segments. Virtual and guest-physical addresses must match,
segments must fit in `[0x10000, 0x40000)`, and permissions are R, RX or RW.
W+X, overlapping pages, invalid offsets/sizes/alignment, unsupported program
headers and entries outside file-backed executable bytes are rejected before
backing-page allocation. This is deliberately not a general ELF loader.

The private GPA range is 256 KiB, with page zero absent. The runtime allocates
63 guest backing pages plus four EPT table pages. The EPT tables and EFI source
file are never mapped into guest physical space. Guest page permissions mirror
the segment R/W/X policy and use NX; the guest EFER includes NXE.

- 0x1000 through 0x4fff: four guest paging levels.
- 0x5000: guest GDT, busy TSS at 0x5100 and empty IDT at 0x5800.
- 0x6000: read-only provisional boot record, passed in RDI.
- 0x7000: RW/NX initial stack, with RSP = 0x8000.
- 0x10000 and above: admitted ELF segments, with zero-filled BSS.

The boot record provides magic, version, size, GPA-range size, initial stack
and image entry. Code begins in supervisor long mode at the ELF entry. The
sample checks that record, its BSS and CPUID, prints a greeting, then exits.

## Bounded runtime contract

The new runtime is linked only into this target and reuses the existing VMX
root probe and entry assembly. Guest CR2 and host DR6 are preserved/restored.
Physical IRQ and NMI exiting, EPT and INVEPT, VM-entry/exit PAT and EFER support,
and the VMX preemption timer are required. Unsupported host translation/debug
state and unsupported VMX controls fail closed without guest execution.

The guest uses integer registers only. The runtime intercepts privileged
operations covered by its controls and all exceptions, but accepts only:

- CPUID: the existing deliberately tiny synthetic profile.
- VMCALL with RAX = 1: write low-byte RBX as console text and resume.
- VMCALL with RAX = 2: stop, with RBX as status; only zero is successful.
- Byte IN/OUT DX to a restricted polled COM1 model, as specified in
  `ms02-guest-uart.md`. All other I/O is rejected, never passed through.

VMCALL console output goes directly through the host diagnostic logger. The
separate UART sample uses an emulated device before reaching the same logger.
NUL has no visible output. Instruction exits must lie wholly within file-backed
executable ELF bytes. Unsupported exits, wrong lengths, entry failures, timer
expiry, malformed state and nonzero guest status do not count as success.
HLT is currently unsupported rather than an implemented sleep/wakeup service.

Execution is limited to 256 entries. Each entry receives at most 2^28 host-TSC
ticks after conversion using IA32_VMX_MISC's timer rate; rates greater than 20
are rejected. This is a tick budget, not a wall-clock duration guarantee.
The guest cannot obtain unbounded execution simply by avoiding VM exits.

The VMCS is cleared and EPT invalidated before returning to the root wrapper.
Only after successful VMXOFF are owned pages released; allocation counts must
return to their baseline. Clear/invalidation failures retain possibly active
ownership and stop. Preparation failures unwind allocations; failed releases
remain recorded rather than disappearing from ownership bookkeeping.

This is a **trusted integer-only prototype**, not an arbitrary/untrusted-code
sandbox. Full architectural-state virtualization, a general CPU/device model,
events and guest interrupt handling, firmware, disk, graphics, scheduling,
multiple vCPUs and an actual guest OS remain outside this prototype.

## Fresh verification and remaining boundary

On 2026-11-12, GCC 14.2.0 warning-clean builds produced the normal ESP, separate
prototype ESP, kernel and guest ELF. ASan/UBSan host tests passed:

- Strict ELF validation, 5,000 deterministic malformed-header mutations,
  executable spans and output-preserving rejection.
- Actual built guest ELF inspection/loading, segment-byte and BSS checks.
- Private bootstrap/EPT/PTE layout and permissions.
- 335 preparation failure injections and 67 failed-release retention/retries.
- Generic exit dispatch, lifecycle, bounds, budgets and atomic rejection.
- Existing boot policy, both kernel-ELF artifacts, V4 boot-info and firmware
  diagnostic adapter regressions.

Independent review and reruns found no further substantive defect within this
scope. Default preprocessed `efi/src/main.c` and `krnl/main.c` are byte-identical
to the baseline without the opt-in macro. The original held MS-01 tests were
not changed or run.

The first image was subsequently executed on archbook QEMU/KVM: a 256 MiB run
completed, while the independent 512 MiB run hit the VMX preemption timer before
printing its greeting, then cleared the VMCS, left VMX and returned failure.
Both results are retained; the first image did not pass both configurations.
R2 increases the per-entry budget from 2^20 to 2^28 TSC ticks while preserving
the 256-entry limit and timer-rate gate. The original budget was too short in
one observed nested-KVM run; this is not a general guest-performance claim.
R2 then passed four independent one-vCPU archbook QEMU 11.1.3/KVM runs, with
256 MiB and 512 MiB each tested twice. Every run used fresh ESP copies and OVMF
variables, no networking and no host disks. All seven required markers appeared
exactly once and in order: external-image load, VMX root entry, guest begin,
guest greeting, zero-status completion, VMX round trip and
`SIMPLE_GUEST.PROTOTYPE_PASS`. All returned 33 without failure markers.

Two additional isolated negative cases also met their failure criteria:

- Corrupt guest ELF magic: rejected before any VMX attempt, no root-entry
  marker, `SIMPLE_GUEST.NOT_COMPLETED`, return 35.
- An independently compiled infinite-loop guest: actual guest entry followed
  by preemption-timer exit reason 52 (0x34), failure, successful VMX round trip,
  `SIMPLE_GUEST.NOT_COMPLETED`, return 35. No success marker appeared.

The original positive guest and the delivered kernel/EFI were unchanged for
the negative cases. Their separate ELF inputs, source, hashes, commands, full
UART output and statuses are retained in the R2 runtime evidence package.
This establishes nested-KVM execution of this technical prototype, not
bare-metal validation, a complete virtual platform or an actual guest OS.
Generic smoke output remains insufficient for acceptance.
The prototype uses a different failure debug-exit status for missing VMX and
other incomplete runs, and does not print a successful generic smoke marker.

The next substantive work is the startup contract and virtual-platform support
for a real simple guest OS. This independently loaded sample does not silently
settle that choice or establish milestone 2 acceptance.
