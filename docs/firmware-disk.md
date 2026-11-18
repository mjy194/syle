# Virtual disk and UEFI-to-Linux acceptance (2026-11-18)

The writable disk-root and reboot-persistence checkpoint is now accepted in
[firmware-persistent-disk.md](firmware-persistent-disk.md). The results below
preserve the earlier read-only/initramfs acceptance.

The experimental `firmware-disk` profile now boots Linux6.12 from its virtual
GPT/FAT32 disk through OVMF and executes commands in an interactive BusyBox
Shell. The final acceptance is
`efi/build/firmware-disk-accepted-20261118/analysis.json` (`PASS`).

The read-only legacy virtio-blk device at PCI 00:03.0 is now connected to the
accepted OVMF/serial runtime. OVMF enumerated it, installed a FAT filesystem,
loaded `EFI/BOOT/BOOTX64.EFI` from its GPT ESP, and that application read and
compared `SYLE.TXT` with its exact expected bytes. These are native device and
filesystem results, superseding the older host-model-only storage status.
The serial function remains at 00:02.0; observed I/O BARs are disk0xc000 and
serial0xc040. The old storage design's proposed 00:02.0 address is superseded.

## Ownership and device subset

The outer Syle EFI loader loads a separate, immutable 64MiB disk image into its
reserved boot-payload allocation. PMM excludes this allocation and guest RAM
is allocated separately. The guest can reach disk bytes only by requests on
its own virtqueue. No host disk, native DMA, host PCI BAR or device passthrough
is used. The existing boot ABI payload ranges transport this image only for
the dedicated `firmware-disk` loader/kernel target. Default loader behavior
and the direct-Linux baseline are retained.

The C block engine and transport are promoted from the existing isolated
prototype: one 128-entry split queue, a single outstanding request, 512-byte
sectors, advertised RO, no indirect descriptors, EVENT_IDX or MSI-X. Requests
must suppress interrupts; an IRQ-requiring completion fails explicitly.
Reads, GET_ID, write rejection and unsupported requests have explicit statuses.
Descriptor ownership, arithmetic, direction, loops and writable aliases are
checked before mutation. Data/status/used-element precede a release fence and
used.idx. Any uncertain memory or RIP commit permanently stops the device.
The RAM adapter maps only this guest's owned low RAM, excluding ROM/VARS/MMIO.
It is synchronous while the vCPU is stopped, and revoked before RAM release.

The deterministic disk builder creates GPT primary/backup headers and a FAT32
ESP without mounting anything. The initial EFI application verifies a small
file; its Linux variant uses standard UEFI LoadImage/StartImage on
`EFI/BOOT/LINUX.EFI`. That image embeds the initramfs. The hypervisor never
inserts a Linux kernel directly into guest RAM, and has no direct-loader
fallback. Writable disks, a Linux-mounted disk root and guest disk IRQs are
separate follow-ups; this first image boots into an in-memory root.

## CPU and firmware handoff work

The firmware-only observer needed additional behavior as Linux took control:

- The LVT mask/unmask transaction preserves timer phase and existing IRR/ISR,
  while masked expirations create no new requests. The unmasked source is
  still the sole existing timer owner. Partial VMWRITE and reentry stop.
- GS_BASE, virtual revision-zero microcode and syscall/FS/kernel-GS MSRs reuse
  existing Linux transaction models and the existing guest state bank.
- A fixed reset-through-Linux CPU profile exposes host-checked PSE/PGE/SYSCALL.
  Its separate mode/memory objects admit their CR4/EFER settings; default
  firmware mode/memory objects retain their original masks. Long-mode MOV-CR
  bytes, VMCS read shadows and hardware fixed-bit projections are validated.
  Reset/protected-mode transitions keep their existing contract.
- INVLPG validates the actual owned instruction and guest operand, invalidates
  the owned EPT, and relies on VPID-disabled VM entry for the linear-cache
  flush. It never executes host INVLPG against a guest address.

The current disk target strips ELF debug sections to retain the original
5MiB file / 8MiB span limits. A strip-proof record verifies all PT_LOAD bytes,
flags and addresses are unchanged. Diagnostic UART writes have a bounded
1000000-poll transport budget; final drain can reprobe a lost debug channel.
No guest clock, VMX slice or memory isolation is relaxed by that transport fix.

## Linux fixture

Linux6.12 source archive is pinned to
`b1a2562be56e42afb3f8489d4c2a7ac472ac23098f1ef1c1e40da601f54625eb`.
`guest/uefi-disk/build-linux.py` is a distinct recipe with EFI stub, PCI/virtio
support and embedded initramfs, preserving historical Linux inputs. GCC16
requires explicit gnu11 in both compressed and EFI-libstub compilation where
the upstream makefiles reset the top-level flags. The initial failed configs
and builds are preserved, and the successful EFI-libstub continuation records
its patch hashes and separate logs. No repeat-build identity is claimed here.

The final EFI app configures `console=ttyS0,115200n8` and
`earlycon=uart8250,io,0x3f8,115200n8`. This explicit ISA COM1 is independent of
the PCI firmware UART. The first boot uses `nokaslr noapic acpi=off pci=off
rdinit=/init` and the embedded initramfs. Linux PCI/virtio drivers are built,
but the accepted run exercises the OVMF block driver; it does not establish a
Linux-mounted disk root or Linux virtio interrupt operation.

## Evidence and current checkpoint

Native file acceptance:
`efi/build/firmware-disk-file-20261117-capture/file-analysis.json`, raw serial,
console transcript, source/media manifests and QMP/process records. It recorded
58 disk completions with no device/backend failure, printed both
`SYLE_DISK_EFI_LOADED` and `SYLE_DISK_FILE_VERIFIED`, then returned to firmware UI.

Final Linux acceptance:
`efi/build/firmware-disk-accepted-20261118/native/positive-once/console.txt`
contains this ordered path:

```text
SYLE_DISK_EFI_LOADED
SYLE_DISK_FILE_VERIFIED
SYLE_DISK_LINUX_START_IMAGE
... Linux version 6.12.0-syle-efi1 ...
... Run /init as init process ...
SYLE_LINUX_USERSPACE_READY
... BusyBox ...
# uname -s
Linux
# uname -r
6.12.0-syle-efi1
# echo SYLE_LINUX_DISK_COMMAND_OK_20261118
SYLE_LINUX_DISK_COMMAND_OK_20261118
#
```

Host input was recorded independently in `input.json`: all59 bytes were
accepted and read by the actual guest UART. There were36 disk request
completions,338 IRQ0 receipts and47 IRQ4 receipts, with no disk/backend
failure. The run completed57697 dispatches in10.4850065 reference seconds,
below the unchanged131072-exit /30-second /2MiB selected-trace limits. Trace
failed/incomplete/dropped are all zero. Tiny and no-Hv controls passed; the
positive process exited cleanly after77.59 wall-clock seconds including trace
drain. This observer deliberately reports its diagnostic stop via exit35;
the ordered evidence analyzer, not that exit code alone, establishes acceptance.

The additional handoff support is explicit:

- PIT2 and CPUID15 use the admitted10MHz reference clock, with no synthetic
  per-exit time advancement.
- Disabled debug-register initialization uses a software bank, exact owned
  MOV-DR decoding, normalized DR6/DR7 and permanent partial-commit failure.
  Active breakpoints remain unsupported and #DB remains intercepted.
- After a guest ICW1 with the firmware timer masked and no pending/reserved
  event, exclusive LINT0 ownership transitions to a routed PIC ExtINT domain.
  Existing PIT0/PIC04/UART models provide IRQ0/IRQ4. Input is admitted before
  reserving an interrupt, and delivery requires a verified native receipt.
- Normal guest exceptions then use VMX delivery (including Linux's INT3
  self-test). #DB/#UD remain intercepted. An unsupported RDMSR CE gets a
  validated #GP(0), with actual IDT/GDT/handler/frame proof and an exclusive
  event lifecycle; it is never fabricated as a successful MSR read.
- User-mode clock/CPUID/preemption paths authenticate flat CPL3 CS/SS.
  Instruction fetching checks cumulative U/S at every page-table level.
  Privileged emulation remains gated to CPL0.

The initial shell test correctly reached a `# ` prompt, while its runner
expected `/ # `; that mismatch and the subsequent unavailable `/proc/sys`
query are preserved as separate attempts. The final test uses the actual
prompt and `uname -r`. No runtime budget increase was needed.

All attempts use fresh private outer VARS/media, tiny/no-Hv controls, one vCPU,
no networking, unchanged paused-QMP device graph admission and bounded cleanup.
Do not infer physical cache-maintenance counts from native attempt counts:
one early capture was lost before WBINVD evidence, one reset-mode rejection
never reached WBINVD, and a no-Hv ELF-size failure prevented its positive run.
The final ledger must distinguish observed from unobserved attempts.


`verification.json` confirms all1589 frozen source-manifest entries and all
three immutable disk copies still match after execution. `cache-ledger.json`
records71 observed architectural L1 WBINVD instructions including the prior
accepted baseline, plus one earlier attempt with lost capture (total71..72).
This is not a physical cache-flush count.

Follow-ups toward a persistent ESXi-style VM are writable backing/storage
lifecycle, a disk interrupt route, Linux PCI/virtio operation and a disk-backed
root filesystem. This checkpoint accepts a read-only boot disk and an
initramfs root, on a single-vCPU diagnostic platform.
