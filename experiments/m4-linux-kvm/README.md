# Linux KVM single-vCPU comparison, 2026-11-18

Both runs booted the same 64 MiB GPT seed, SHA-256
`9cdef79a674441fce3e431d93ff99f12b8a20d4c59997489af25fb6b501658f0`,
and ran the probe from its ext2 root after devtmpfs was mounted. The reference
used a disposable QEMU disk snapshot; Syle used a new writable copy.
The guest kernel SHA-256 was
`9e7fcba4547af5e4b577fd12828c50fbc1176262263b0c8526e136b13f241116`.

The Linux build copied the M2 graphics checkpoint object tree into a fresh
directory, enabled VIRTUALIZATION, KVM and KVM_INTEL, disabled KVM_WERROR and
ran olddefconfig and bzImage. The resolved config has KVM_X86=y, KVM_INTEL=y,
KVM=y and SMP disabled. The disk uses the M2 ext2 filesystem, the root-disk
UEFI loader built with SYLE_DISK_LINUX and SYLE_DISK_ROOT, and l2-probe
installed as /bin/l2-probe and called by /sbin/init. Build the probe with:

```sh
musl-gcc -static -Os -Wall -Wextra -Werror -idirafter /usr/include l2-probe.c -o l2-probe
```

* Reference: QEMU pc/KVM, host CPU, one vCPU, 128 MiB, OVMF, legacy virtio-blk.
  Linux reported VMX=1. The same vCPU returned I/O exits for A and B and
  then HLT. B was calculated from the previous AL after re-entry. The probe
  exited zero. The outer reference harness terminated QEMU on its 25-second
  budget after the shell appeared (timeout 124).
* Syle: unmodified M2 Ring3 management runtime under QEMU/KVM, one guest vCPU.
  Linux booted and mounted the same root. It logged
  `kvm_intel: VMX not supported by CPU 0`. CPUID.1:ECX was zero and opening
  /dev/kvm returned ENOENT. No CREATE_VM or KVM_RUN was reached. After guest
  destruction, allocated pages returned to 262, VMX was inactive and management
  exited zero. This isolates the first blocker to the CPU profile exposed to
  Linux. Later nested entry, memory and event behavior remain untested on this path.

Evidence is under efi/build/m4-kvm-l1 in reference.log, syle/serial.log,
syle/command.json, objects/.config and disk.json. The Syle run logged one L1
WBINVD attempt, bringing the previous M2 ledger to 86 observed with an upper
bound of 87 (one historical lost capture).

## Guest-selectable KVM initialization

Build M2 with `python3 m2/build.py OUT EFI_PREFIX GUEST_OVMF_CODE GUEST_OVMF_VARS --nested`
to include the experimental VMX initialization profile. At the Ring3 menu,
`c` creates the ordinary M2 guest and `n` creates a guest with nested VMX
enabled. The selection lasts until destruction. Use `s` to start and `a`
to attach. Builds without the option expose only the ordinary create operation.

With the same KVM-enabled kernel, install the updated probe on the ext2 root.
For device acceptance invoke `/bin/l2-probe --check-device` from init and
report its exit status with `printf 'KVM_PROBE_DONE rc=%d\n' "$?"`.
This checks the real open and KVM_GET_API_VERSION ioctl. On a run started
with `m2/run.py --socket`, execute
`python3 experiments/m4-linux-kvm/check-switch.py RUN_DIRECTORY` to verify
nested, ordinary, then nested creation in one host session. Each guest is
destroyed before the next is created.

The initial device-only checkpoint stopped at VMCLEAR during vCPU creation.
The full-entry run below uses the subsequent VMCS02 implementation.

The acceptance run is recorded in `efi/build/m4-init-4/switch/serial.log`.
The same host and disk returned API 12, ENOENT, and API 12 in three successive
guest generations; every release returned to 262 pages. The disk SHA-256 was
`bb65ddf37d47234c8fcd6d76cfcdf75bd85c271b378666d94effe1d94dec0654`.
The three development boots and three acceptance boots each logged one L1
WBINVD attempt; the cumulative ledger is 92 observed with upper bound 93.


## Minimal L2 entry, exit and resume

Boot the full-probe seed described above, with init invoking `/bin/l2-probe`
without arguments. Build with `--nested`, start M2 with `--socket` and use
`python3 experiments/m4-linux-kvm/check-switch.py RUN_DIRECTORY --full-l2`.
The checker remounts the disk read-only before each destroy and checks the
page baseline and VMX, EPT and disk-adapter teardown.

In `efi/build/m4-init-vmcs12/native/serial.log`, nested generations 1 and 3
both reported I/O exits for A and B on port 0xe9 and a third KVM_RUN with
exit reason 5 (HLT). The L2 increments AX after the first exit. Generation 2
reported VMX=0 and ENOENT opening /dev/kvm. All three releases returned to
262 pages and management exited zero. Across the eleven development boots
and the three-generation acceptance run the ledger reached 106 observed L1
WBINVD attempts, with a conservative upper bound of 107.

Scope is one L1 Linux vCPU, one virtual VMCS and the real-mode probe. The
adapter creates a private hardware VMCS02 at each virtual entry, fills a
sparse EPT02 from L1 RAM on demand, and invalidates it at teardown. Linux
handles the intercepted general protection fault for the VM86 HLT. The
profile excludes pending event injection and guest-controlled MSR lists.
The next checkpoint covers direct entry into a 64-bit payload.


## Single-vCPU 64-bit L2

Use the same KVM-enabled Linux kernel and build the probe as above. Install it
on the ext2 root and invoke `/bin/l2-probe --long-mode` from init. For a Syle
run with a serial socket, use
`python3 experiments/m4-linux-kvm/check-switch.py RUN_DIRECTORY --long-mode`.
The reference and Syle runs used the same seed image, SHA-256
`0a20bfaa0abb3a85716bcd5793d6ec6bba3fc17e501f386e63ad4eac9ab1b3c4`.

The probe configures the vCPU with KVM_GET_SUPPORTED_CPUID and KVM_SET_CPUID2,
maps its code through four levels of 4 KiB page tables and sets long-mode CS,
CR0.PE/PG, CR4.PAE and EFER.LME/LMA before KVM_RUN. The first I/O exit emits
A. After re-entry the L2 increments R8, copies it to RAX and emits B. A third
KVM_RUN returns KVM_EXIT_HLT. KVM_GET_REGS must return RIP 0x18 and
R8 = RAX = 0x1234567800000042.

The QEMU/KVM reference transcript is in
`efi/build/m4-long64/reference-cpuid.log`. The original Syle profile rejected
VM-entry controls 0x204 and guest CR4 0x2020 at VMLAUNCH, before physical
entry. The updated adapter selects IA-32e VMCS02 entry, checks the guest code
segment and VM flag, and prepares guest EFER for the long-mode profile.
`efi/build/m4-init-long1/long64/serial.log` records the full sequence and
the same final registers as the reference. This covers direct 64-bit entry
for a minimal payload. The following Linux checkpoint extends this direct-entry profile.

In the same management session, generations 1 and 3 completed the 64-bit
probe and ordinary generation 2 observed VMX=0 and ENOENT for /dev/kvm.
Each release returned to 262 pages. The session ended with management status
zero. See `efi/build/m4-init-long1/accept64/serial.log`. The real-mode
regression on the same build also passed (`m4-init-long1/native/serial.log`).
The six additional Syle boots each recorded one L1 WBINVD attempt, bringing
the ledger to 112 observed, with a conservative upper bound of 113.

## Single-vCPU L2 Linux direct64

Build the static L1 runner from the checkout root. This uses the same Linux
boot loader as the native profile with 48 MiB of L2 RAM.

```sh
musl-gcc -static -Os -Wall -Wextra -Werror -idirafter /usr/include \
  -Ikrnl -DLB_RAM_SIZE=50331648 experiments/m4-linux-kvm/l2-linux.c \
  experiments/m4-linux-kvm/l2-disk.c krnl/virtio_blk/rw_backend.c krnl/linux_boot.c -o l2-linux
./l2-linux bzImage initramfs.cpio
```

Install the runner at `/bin/l2-linux` and put the bzImage and newc archive
at `/bzImage` and `/initrd` on the L1 disk. L1 init runs the same command
on both machines:

```sh
/bin/l2-linux /bzImage /initrd 2>&1
printf 'KVM_PROBE_DONE rc=%d\n' "$?"
```

The machine uses the in-kernel KVM PIC and PIT with speaker-port support,
coalesces late PIT edges, and provides COM1 and a fixed CMOS boot date.
L2 gets these arguments directly from the runner:

```text
earlycon=uart8250,io,0x3f8,115200n8 console=ttyS0,115200n8 rdinit=/init nokaslr noapic nolapic acpi=off pci=off tsc=reliable lpj=1000000
```

### Paired acceptance with Linux 6.12 and HZ=100

Both runs used byte-identical artifacts from `efi/build/m4-direct-hz100`.

| Input | SHA-256 |
| --- | --- |
| L2 bzImage | `e3e130ff2f0cb4192180e99eb96256ca1818f311be6677795737e01413aebdbd` |
| L2 initramfs | `7f71f66ed40ca0fa5e26e9dba8138672c148b0f720ed856a118b8955c36fa0a5` |
| L1 static runner | `e65a603817767eb853d17762878abc09b758e56dd20bd49788c0d180bae030a0` |
| L1 disk seed | `7813132e63bb291bb0db87e843d1d80dcb6d8538c8edb6a7509409e72d243727` |

The 100 Hz kernel is a fresh build of the single-CPU Linux 6.12 rebuild
fixture. From its resolved config, disable HZ_250, enable HZ_100, run
`make olddefconfig` and build `bzImage` in a fresh object directory. The
exact config is in `objects/.config`, SHA-256
`45f7dbcefaaffe1deda01d188a2d4db8f24487df32871c73c887aeea831acd06`.
The userspace marker comes from PID 1 after mounting proc, sysfs and devtmpfs.

* **Reference** — QEMU/KVM pc, host CPU, one vCPU, 128 MiB, M2 KVM-enabled L1
  Linux booted with `console=ttyS0,115200n8 root=/dev/vda2 rootfstype=ext2 rw rootwait init=/sbin/init idle=poll`.
  The runner reported earlycon, initramfs unpack, Run /init,
  SYLE_LINUX_USERSPACE_READY and exit code zero in `ref1.log`.
* **Syle** — M2 with `--nested`, UEFI disk boot, Ring3 nested creation.
  Start `m2/run.py --socket` with a fresh disk copied from that seed and run
  `python3 experiments/m4-linux-kvm/check-linux.py RUN_DIRECTORY`.
  The transcript at `efi/build/m4-init-boot14/linux100/serial.log` records
  the same L2 markers, a clean read-only remount of the L1 root, and release
  back to 262 pages with VMX inactive and EPT released. Management exited zero.

### Original 250 Hz kernel

The reference KVM run and Syle use one vCPU and the same disk seed
(`efi/build/m4-direct-tick/seed.img`, SHA-256
`ec040e1ff1ea099a661de0358e9b8ceb7f45d67199d437e56245c61f09fe2a24`).
The seed contains the recovered bzImage
`3d1c081c18e64ad97a44e2b038b8b3d0c733d38494b6293ba2402898c3a629e6`
and initramfs `7f71f66ed40ca0fa5e26e9dba8138672c148b0f720ed856a118b8955c36fa0a5`.
Both use the direct64 command line above and the same PIT reinjection setting.

The L2 i8259 handler masks IRQ0, sends specific EOI (port 0x20, value
0x60), then unmasks IRQ0 at port 0x21. Each port exit returns to L1 KVM,
which reads and updates VMCS12 before resuming L2. The table uses Syle's
10 MHz clock sampled at entry dispatch, from dispatch of the injected vector
0x30 to dispatch of the entry that exits on unmask. This measures elapsed
time across the nested transitions. It excludes the rest of the handler
after unmask and includes time spent in L1 and Syle.

| Syle build | Completed sequences | Mean elapsed | At least 4 ms | Mean L1 VMX instructions |
| --- | ---: | ---: | ---: | ---: |
| Instrumented baseline (`m4-init-irqbase`) | 1,024 | 4.95 ms | 1,019 | 28.23 |
| Register operands from VM-exit information and page-wise instruction fetch (`m4-init-irqfast`) | 1,024 | 3.47 ms | 137 | 27.85 |
| Also retain VMCS02 across reflected exits (`m4-init-irqcache`) | 1,024 | 3.22 ms | 39 | 27.80 |
| Also group up to eight register accesses while L1 interrupts are disabled (`m4-init-irq250final`) | 1,024 | 2.64 ms | 26 | 27.65 |

Read the counters in each build's `accept-linux/serial.log` under `efi/build`.
These are single-run measurements. VMCS02 is cleared before stop, teardown,
virtual VMXOFF or VMCLEAR. Every hardware entry in the group still goes
through the normal host state save and restore, and counts against the
management slice.

The reference transcript is `efi/build/m4-direct-tick/refirq250.log`.
Syle's `efi/build/m4-init-irq250final/accept-linux/serial.log` records earlycon,
initramfs unpack, `Run /init as init process`, the PID 1 userspace marker,
`L2_LINUX_INIT_PASS`, and guest destruction with 262 pages before and after.

The adapter supports direct64 entry, KVM CR masks/read shadows, virtual
TSC and disabled debug-register access. It preserves in-flight supported
events across internal EPT fills and reflects EPT12 MMIO misconfigurations
to KVM. Composed translations are cached for one EPTP12 and cleared on
virtual INVEPT, VMXOFF, VMCLEAR, a source change, managed stop or teardown.
The 250 Hz build also passed the real-mode probe and the 64-bit
nested/ordinary/nested switch test. The earlier 100 Hz checkpoint recorded
133 cumulative L1 WBINVD attempts with an upper bound of 134. The 14 Syle
boots for the IRQ0 comparison and regressions each recorded one additional
attempt, bringing the total to 147 observed with an upper bound of 148.

### Interactive shell on the original 250 Hz kernel

On the L1 serial terminal run `/bin/l2-linux /bzImage /initrd --shell`.
The recovered initramfs already launches BusyBox ash on ttyS0. Shell mode
keeps the vCPU running after the PID 1 marker, forwards the prompt immediately,
and routes terminal input through COM1's receive register and IRQ4. The L1
terminal settings are restored when the runner exits.

To boot the prepared disk interactively from the checkout root, use a new
run directory and writable disk. Press `n`, `s`, then `a` in the Ring3 menu.

```sh
python3 m2/run.py efi/build/m4-init-irq250final NEW_RUN \
  --seed efi/build/m4-shell250/seed.img --disk NEW_DISK.img --vars HOST_OVMF_VARS
```

Run `cat /proc/cmdline` at the L2 prompt to see `rdinit=/init`.
Use `poweroff -f` to invoke Linux shutdown directly with this shell-based
PID 1. With ACPI disabled Linux completes device shutdown and halts. After
the kernel's shutdown message the runner verifies `KVM_MP_STATE_HALTED`
and cleared RFLAGS.IF, then exits and releases the L2 VM. L1 remains available.

For automated acceptance, add `--socket` to the launch and run
`python3 experiments/m4-linux-kvm/check-linux.py NEW_RUN --shell` from
another terminal. It checks a randomized reply computed from L2's
`/proc/1/comm`, the L2 command line, shutdown, and Ring3 resource release.

The reference and Syle acceptance transcripts are
`efi/build/m4-shell250/ref2/serial.log` and
`efi/build/m4-shell250/syle2/serial.log`. Both used seed SHA-256
`4c6411e9853a024dd7cfe92742978e3d600070b33d9b78175f93f369d3c5f759`.
The L2 kernel and initramfs extracted from that disk match the original
250 Hz artifacts byte for byte. Both Syle shell runs recorded one additional
L1 WBINVD attempt, bringing the cumulative ledger to 149 observed with an
upper bound of 150.

### L2 Linux with a writable disk root

The recovered 250 Hz config disables CONFIG_BLOCK. The disk kernel uses the
Linux 6.12 single-vCPU rebuild config at 250 Hz with BLOCK, BLK_DEV,
VIRTIO_MENU, VIRTIO_MMIO, VIRTIO_MMIO_CMDLINE_DEVICES, VIRTIO_BLK and EXT2_FS
built in. The direct64 loader receives an empty newc archive. Linux discovers
virtio-mmio at 0xd0000000 on IRQ5 and mounts the whole 8 MiB ext2 device
as `root=/dev/vda rootfstype=ext2 rw`. BusyBox and `/sbin/init` live on disk.

From the L1 serial console run:

```sh
/bin/l2-linux /bzImage /initrd --disk /l2.ext2
```

To prepare a new root, run
`python3 experiments/m4-linux-kvm/make-root.py ORIGINAL_INITRAMFS NEW_DIRECTORY`.
Install its `l2.ext2` and `empty.cpio` files on L1 as `/l2.ext2` and `/initrd`.
The kernel configuration used for acceptance is
`efi/build/m4-l2-root/objects/.config`.

Here `/initrd` is the empty archive. The legacy MMIO transport uses one
128-entry split queue and reuses the M2 read/write block engine. Sector
writes and FLUSH requests reach the L1 regular file through fdatasync
before successful completion is published to L2. The image is opened
exclusively. On an L1 without flock, the runner owns an inode-specific
lock file in devtmpfs. An unclean runner death on that profile leaves
the lock until L1 reboots or an operator releases it after checking ownership.

The prepared seed at `efi/build/m4-l2-root/seed.img` starts two independent
L2 runner processes in succession on the same L1 backing file. Launch the
existing nested Syle build with this seed and a new writable L1 disk:

```sh
python3 m2/run.py efi/build/m4-init-disk250v2 NEW_RUN \
  --seed efi/build/m4-l2-root/seed.img --disk NEW_L1_DISK.img --vars HOST_OVMF_VARS --socket
```

From another terminal, run
`python3 experiments/m4-linux-kvm/check-linux.py NEW_RUN --disk`.
Both boots must report an ext2 root mounted read/write. The first shell writes
a random value to `/root/probe`. Both shells read it back, remount the root
read-only, and power off. The second boot learns the expected value only in
the external verifier. After the second L2 halt, the verifier remounts L1
read-only and checks management teardown.

The reference transcript is `efi/build/m4-l2-root/ref3/serial.log`.
Syle's `efi/build/m4-l2-root/syle4/serial.log` records two ext2 mounts on
device 254:0, both shutdowns, and the same token on both boots. Both
environments started with seed SHA-256
`b3b1802510ef1aedd341cbb024defde20f3bc8e86ecc422c686d7089c50e90ad`.
After outer guest teardown, read-only e2fsck passed for both ext2 filesystems.
The extracted L2 image and offline inspection are in `syle4/l2-after.ext2`
and `syle4/offline.log` beside the serial transcript.

The second KVM process can allocate a different VMCS12 page within the
same L1. Syle now saves the virtual fields into L1 RAM when clearing that
region and binds the single inactive slot to the next region. A fresh page
starts with empty fields. Reloading a cleared page restores the saved fields;
L0 retains ownership of all physical VMCS and EPT state. The seven additional
Syle guest boots bring the L1 WBINVD ledger to 156 observed (upper bound 157).
