# Ring3-managed UEFI and writable disk accepted (2026-11-18)

The fixed UEFI + writable virtio-blk runtime now runs under the existing Ring3
management ABI. The native lifecycle acceptance is
`efi/build/managed-firmware-remount-20261118/analysis.json` (`PASS`). In one
outer QEMU/Syle instance, Ring3 boots Linux from the disk, pauses and resumes
the same guest, destroys it, boots a fresh guest using the same backing file,
checks its persisted data, and destroys it again. Both releases restore the
exact PMM page balance. No host serial command is injected.

## Runtime and ownership

`krnl/firmware_managed.inc` is the lifecycle adapter around the frozen,
previously accepted persistent firmware engine. `prepare.py` verifies and
copies that engine's complete source manifest before splitting its continuous
loop. It preserves the existing per-entry clock, IRQ receipt, HLT and device
transactions. The ordinary checkout observer remains an earlier profile;
build this integration from its prepared source, not by replacing the frozen
engine with the checkout observer.

CREATE prepares one fixed firmware/disk profile without entering it. START
and STEP each perform at most 64 entry/exit transactions and return to Ring3.
STOP performs no guest entry, flushes the backing store, and retains VMCS,
RAM, GPRs, FX/MSR state and all devices. Before resuming, the adapter verifies
the current VMCS identity, exit count, and a diagnostic digest of VMCS guest
fields, saved CPU state, all 128MiB RAM, flash, EPT tables and device state.
This digest checks accidental mutation; it is not a cryptographic guarantee.
The accepted host clock is sampled only while running. Elapsed wall time is
observed on resumption under the existing 60-second maximum sampling gap;
this implementation does not freeze virtual time during an arbitrary pause.

All native callbacks enter the existing trusted supervisor-root context and
normalize DS/ES/SS before PMM/VMX work, then restore the exact int80 caller
context. The manager is an immutable embedded ELF loaded through the strict
Ring3 ELF loader, with the usual separate code/data/stack mappings. The boot
payload remains the 64MiB disk image. Ring3 can select only the existing fixed
boot profile, never supply arbitrary images or host pointers.

Console SEND/SPACE and RECV use bounded 16-byte RX and 4096-byte TX queues.
The firmware profile consumes no host UART input; Ring3 forwards guest output
through its existing output syscall. RX is delivered before IRQ reservation,
using the same UART/PIC path as the earlier accepted runtime.

DESTROY closes and flushes the synchronous PIO provider, executes the existing
VMCLEAR/INVEPT/VMXOFF teardown, restores host CR4, revokes the disk RAM adapter,
and releases firmware RAM, flash and EPT pages. The PMM allocated-page count
must equal the CREATE baseline. A failed teardown retains ownership through
the existing controller's quarantine path. Only after complete release can
fresh-domain initialization clear engine/device state for the next handle.
The PIO driver now supports closing and rebinding the healthy fixed backend.

The runtime retains its 1,048,576-exit / 120-reference-second budget per guest
and 2MiB selected trace buffer. Successful teardown emits aggregate trace
integrity counters without serializing the whole retained trace inside the
management syscall. Full selected trace contents are not an acceptance
artifact for this lifecycle profile.

## Native acceptance

The manager used CREATE/START/STEP/STOP/STATUS/DESTROY and console operations
through int80. It wrote and fsynced a new disk token, then executed STOP,
256 pure STATUS calls and a STEP that had to return `-ESTATE`. START used the
same handle; before entry its exit count remained 70,656 and its state digest
remained `0x465982f59a7d4983`. The resumed shell checked its retained `m2token`
variable and original `$$` PID, verified the file, and remounted the real ext2
root read-only. The exact response `SYLE_LIFE_RESUMED` was observed.

After DESTROY, the old handle had to return `-ENOENT`. A different handle
booted OVMF and Linux again, mounted `vda2`, and verified the same file without
rewriting it. It remounted root read-only before STOP/DESTROY. The second old
handle was also invalid. Ring3 printed `FIRMWARE_LIFECYCLE_PASS`, exited with
status zero, and the test QEMU exited with code 33 in 36.17 seconds.

| Native check | Guest generation 1 | Guest generation 2 |
| --- | ---: | ---: |
| Allocated pages during CREATE | +33,864 | +33,864 |
| PMM pages before CREATE / after DESTROY | 262 / 262 | 262 / 262 |
| Guest sector reads / writes | 7,175 / 110 | 7,173 / 40 |
| Guest FLUSH requests / IRQ5 receipts | 148 / 59 | 62 / 52 |
| Ring3 RX sent / delivered | 272 / 272 | 122 / 122 |
| Guest TX produced / Ring3 received | 22,646 / 22,646 | 22,560 / 22,560 |
| VMX active / VMXON page / VMCS page after release | 0 / 0 / 0 | 0 / 0 / 0 |
| EPT released / adapter revoked | 1 / 1 | 1 / 1 |
| Selected trace failures / dropped bytes | 0 / 0 | 0 / 0 |

The allocator reused the same VMCS/EPT physical addresses after the first
release. Handle generations, full release and fresh initialization distinguish
the newly booted guest; address inequality is not required.

Offline `debugfs` verified the exact 33-byte `/persist/probe.txt` and `e2fsck
-fn` returned zero. File SHA-256:
`7f720e6bdc39be3d36e8b14579f42ddd192bdca3b2dd582e19cbf660e3c4175c`.
The seed disk does not contain the token. The admitted raw backing file stayed
open in one positive QEMU process (PID 779240), inode 1815108. Tiny firmware
and no-Hv-clock controls passed without executing L1 WBINVD. The positive
run retained the fixed paused-QMP graph and previously pinned host/module
admission, with only the explicit PIIX3 PIO fixture extension.

The first native attempt already passed same-guest restoration, then stopped
at BusyBox's `mount -o remount,ro /` returning EINVAL for `/dev/root`. That
attempt was terminated and preserved. Explicitly naming `-t ext2` and
`/dev/vda2` fixed the acceptance command; the runtime was unchanged. The failed
attempt observed one L1 WBINVD and the accepted run observed two. Combined
with the earlier ledger, the cumulative count is 78 observed, with one older
lost-capture uncertainty: 78..79. Use **79** as the next conservative prior
upper bound. These count architectural L1 instructions, not physical cache
flushes.

## Reproduction and remaining scope

See `experiments/m2-snapshots/managed-firmware/README.md` for preparation,
building, native execution, evidence analysis and host tests. The accepted
kernel ELF SHA-256 is
`ebbcb1f6a12865da135dcb5197348c9df1ccc681b58cc1d18f9bf6d62cf29677`;
the embedded manager ELF SHA-256 is
`c3a11ab3cff1aec6242265a27804defbac41d80970e03682a1353818b4d0b383`.
The 1,604-file source manifest and native media manifests were verified.

This closes the requested Ring3 lifecycle integration for the admitted
single-BSP, single-vCPU UEFI/disk profile. Milestone 2 still needs the guest
GOP/framebuffer and host display path. General multi-VM scheduling, arbitrary
guest image management, unbounded suspension and live migration are separate
work; they are not acceptance claims here.


## Display follow-up accepted (2026-11-18)

The display gap above is now closed for this fixed profile. Actual OVMF GOP
and Linux `/dev/fb0` pixels reach the host framebuffer and pass independent
QMP screenshot checks while all lifecycle and disk checks remain intact.
See [firmware-graphics.md](firmware-graphics.md) for the separate acceptance.
