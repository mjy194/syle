# Writable disk root and reboot persistence accepted (2026-11-18)

The experimental persistent profile now boots Linux6.12 directly from its
virtio-blk ext2 root partition, writes and fsyncs a file, requests a reboot,
and verifies that file in a completely new QEMU/Syle/Linux instance using the
same backing file. The final acceptance is
`efi/build/persistent-accepted-20261118/analysis.json` (`PASS`).

## Accepted behavior

The64MiB disk contains a36MiB GPT/FAT32 ESP and an ext2 root partition at
LBA75776..131038. OVMF reads the ESP, loads the EFI application and Linux EFI
stub, and Linux's own legacy virtio PCI driver discovers `vda1/vda2` and mounts
`vda2`. The new kernel has `CONFIG_INITRAMFS_SOURCE=""`; neither an embedded
root archive nor an external initrd is supplied. Its command line is:

```text
console=ttyS0,115200n8 earlycon=uart8250,io,0x3f8,115200n8
nokaslr noapic acpi=off pci=conf1 root=/dev/vda2 rootfstype=ext2
rw rootwait init=/sbin/init
```

The first boot prints:

```text
virtio_blk virtio0: [vda] 131072 512-byte logical blocks
vda: vda1 vda2
VFS: Mounted root (ext2 filesystem) on device 254:2.
Run /sbin/init as init process
SYLE_DISK_ROOT_READY
/dev/root / ext2 rw,relatime,errors=continue 0 0
# persistctl write c43dfd92069e87b9c5a735277d85592f
SYLE_PERSIST_WRITTEN c43dfd92069e87b9c5a735277d85592f
# persistctl reboot
SYLE_PERSIST_ROOT_REMOUNTED_RO
SYLE_PERSIST_REBOOT_REQUEST
```

`persistctl` is a small freestanding Linux userspace test program installed
on the disk. It checks the root filesystem type, writes `/persist/probe.txt`,
fsyncs the file and its directory, then uses the Linux mount/reboot syscalls
to remount the root read-only and request restart. The VMM authenticates the
actual reset-port instruction and flushes the backing store before stopping.
The reboot test destroys that QEMU/L1/L2 and starts a new one with fresh guest
memory, VMCS and outer VARS. It reuses the same disk path/device/inode; it does
not restore a RAM snapshot or copy data out of the old guest into the new one.
This is a managed cold restart, not an in-process VMCS reset implementation.

The second boot mounts the disk root again and executes:

```text
# persistctl read c43dfd92069e87b9c5a735277d85592f
SYLE_PERSIST_VERIFIED c43dfd92069e87b9c5a735277d85592f
```

The token was generated after the initial disk was built and was absent from
that seed. The entire first post-reboot disk hash equals the second process's
pre-boot disk hash. Independent `debugfs` extraction from both disk snapshots
returns exactly the same33 file bytes, with SHA-256
`3f8355ec20b05695d8dcbe6bf8bb705269d8b954c76d0f2434a6067ea1dd2fe0`.
Both offline `e2fsck -fn` checks return0.

| Native checkpoint | First/write boot | Fresh/read boot |
|---|---:|---:|
| QEMU PID |371681|374905|
| Backend sectors read |7175|7173|
| Backend sectors written |122|54|
| Backend flushes |154|69|
| Proven IRQ5 receipts |69|62|
| Completed dispatches |66699|64298|
| Reference seconds |16.9685631|15.8513624|

The second boot's writes are ordinary firmware/filesystem metadata updates;
file contents are compared independently of the changing whole-disk hash.
Tiny/no-Hv controls pass for both launches. Disk/backend errors, trace
failed/incomplete/dropped counts and input loss are all zero. The same kernel
binary (`17072eefbd14bbbf1610734847441d2e5ae2759e50426a3b589f65129e7009cd`)
is used for both starts, and all1597 source-manifest entries verify afterward
in each stage. The diagnostic observer exits35 after its reset checkpoint;
that exit code alone is not a boot/persistence acceptance signal.

## Storage and interrupt ownership

The guest device stays at PCI00:03.0, BAR0xc000. A separate writable legacy
transport advertises FLUSH and fixed INTx IRQ5. It handles IN, OUT, FLUSH and
GET_ID; it drains at most128 available heads per notification, without
indirect descriptors, EVENT_IDX, MSI-X or multiqueue. Descriptor directions,
lengths, sector bounds, loops, ring ownership and writable aliases are checked
before mutation. Scatter segments may cross sector boundaries. Every OUT
flushes stable backing before its status/used-index completion is published.
Unknown partial I/O or publication failures permanently stop the device;
reset cannot clear that terminal failure. The prior read-only transport and
its write-rejection tests remain separate.

The actual persistence provider is a separately admitted native PIO driver.
For this KVM fixture it owns only a dedicated outer QEMU PIIX3 IDE function at
00:05.0, primary master, with serial `SYLE-PERSIST-0001` and exactly64MiB
capacity. It attests the virtual PCI identity, disables bus-master DMA and
ATA interrupts, and uses bounded LBA28 CPU PIO plus ATA FLUSH CACHE. It is not
a physical disk passthrough driver. The runner admits exactly this additional
virtual device, validates the raw writable backing file and enabled cache
flushes, and records the matching writable file descriptor and inode before
continuing the paused VM. No network/VFIO/vhost device is added.

The existing EFI boot-payload allocation is retained for size/ABI admission;
the persistent device does not read sectors from that RAM seed. All sectors,
including the ESP/kernel and root filesystem, use the PIO-backed callbacks.
The complete backing file survives even destruction of the outer VM.

IRQ5 is an opt-in extension of the master PIC model alongside IRQ0/IRQ4. It
preserves priority nesting, latched requests, reservation generations and
EOI behavior, and shares the existing proven native-entry receipt path.
Device ISR acknowledgement deasserts its PIC input; it never fabricates an
interrupt receipt. The source-free firmware LINT0 contract transitions to
routed PIC ExtINT only after its timer/event owner is quiescent. Linux's PCI
configuration type1 selection probe at0xCFB is now modeled separately from
the CF8 address DWORD.

The persistent fixture has explicit upper bounds of1048576 exits,120 reference
seconds,2MiB selected trace and300 outer wall-clock seconds; the previous
firmware/readonly profiles retain their own bounds. Both accepted runs finish
well below these limits. This accepts clean-reboot persistence, not arbitrary
power-loss recovery or a general multiprocessor/storage-management platform.

## Reproduction and evidence

`guest/uefi-disk/build-linux.py --disk-root` creates the separate EFI-stub/root
kernel configuration. `make-rootfs.py` builds ext2 without mounting a host
filesystem, reuses the pinned static BusyBox, and installs the test helper and
console device nodes. The root filesystem's `/sbin/init` mounts proc/sysfs/
devtmpfs and starts the interactive Shell. `make-disk.py --rootfs ROOT.ext2`
adds its GPT partition; build the EFI application with
`UEFI_DISK_CFLAGS='-DSYLE_DISK_LINUX -DSYLE_DISK_ROOT'`.

Prepare a new candidate from the accepted firmware runtime:

```sh
python3 experiments/m2-snapshots/persistent-progress/prepare.py --output CANDIDATE
# Place the new64MiB disk.img and its disk.json in CANDIDATE.
sh experiments/m2-snapshots/persistent-progress/build.sh \
  CANDIDATE GNU_EFI_PREFIX OVMF_CODE OVMF_VARS
sh boot/tests/kernel_elf.sh CANDIDATE/source/efi/build/krnl-firmware-disk.elf
python3 experiments/m2-snapshots/persistent-progress/validate-pair.py \
  CANDIDATE --linux-config ROOT_KERNEL/resolved.config --ledger PRIOR_UPPER_BOUND
```

The orchestration script creates a fresh backing copy and random token,
validates the write/reset boot, captures an offline snapshot, then recreates
the VM for read/reset verification on the same file. Historical fixture/media
paths, host KVM/module pins and source hashes remain explicit in the native
preparer; it fails if they no longer match. Never reuse an attempted run.

The acceptance can be independently rechecked without KVM:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/analyze_persistent_disk.py \
  efi/build/persistent-accepted-20261118
```

The analyzer requires ordered firmware/kernel/disk-root/user markers, exact
responses distinct from terminal echo, complete TX/input counters, IRQ5
receipts, actual guest reset, identical backing identity and hash continuity,
separate QEMU processes, an initramfs-free config, exact offline file content
and two successful fsck checks. Eight host evidence tests reject missing
results, IRQ/reset failures, timeouts and partial capture. Sanitizer tests
cover write/flush failures, bounds, scatter/batch publication and durable
reopen; default PIC04 and read-only storage regressions also pass.


## Ring3 lifecycle follow-up (2026-11-18)

The same admitted UEFI + writable disk engine now runs through Ring3 management.
Same-guest STOP/START preserves the shell PID and variables; DESTROY and a new
CREATE boot Linux again from the persisted file. Both releases restore exact
PMM page balance and clear VMX/EPT/adapter ownership. See
[firmware-managed.md](firmware-managed.md) for the independent acceptance.
