# Persistent virtual disk checkpoint

Acceptance: `efi/build/persistent-accepted-20261118/analysis.json`. See
`docs/firmware-persistent-disk.md` for exact scope and native counters.

Use a newly prepared source directory for every runtime change. `prepare.py`
verifies the complete earlier accepted disk-runtime manifest, copies its
frozen platform overlays, then adds the writable provider, IRQ5 and reset
checkpoint. The ordinary checkout observer is an older build-check profile;
the prepared source is the native runtime. Keep distinct build directories
for read-only and persistent profiles (their compile flags differ).

`build.sh CANDIDATE GNU_EFI_PREFIX OVMF_CODE OVMF_VARS` compiles that prepared
source with the dedicated persistent makefile. Run the production ELF loader
check before staging native media. Use `guest/uefi-disk/build-linux.py
--disk-root` for an EFI-stub kernel with an empty INITRAMFS_SOURCE and EXT2_FS.
Build the freestanding guest helper (never execute its reboot operation on
the development host):

```sh
cc -O2 -static -nostdlib -fno-pie -no-pie -fno-stack-protector \
  -fno-builtin -mgeneral-regs-only -mno-red-zone -Wall -Wextra -Werror \
  guest/uefi-disk/persist-probe.c -o OUTPUT/persistctl
python3 guest/uefi-disk/make-rootfs.py --initramfs VERIFIED_OLD_ARCHIVE \
  --probe OUTPUT/persistctl --output NEW_ROOTFS_DIRECTORY
UEFI_DISK_CFLAGS='-DSYLE_DISK_LINUX -DSYLE_DISK_ROOT' \
  sh guest/uefi-disk/build.sh EFI_OUTPUT GNU_EFI_PREFIX
python3 guest/uefi-disk/make-disk.py --efi EFI_OUTPUT/BOOTX64.EFI \
  --linux ROOT_KERNEL/LINUX.EFI --rootfs NEW_ROOTFS_DIRECTORY/root.ext2 \
  --output CANDIDATE/disk.img > CANDIDATE/disk.json
```

The old archive is used only as a source of the already verified static
BusyBox ELF. No archive is embedded in the new kernel or unpacked at boot.
The ext2 builder uses mke2fs/debugfs without host mounts or privileged mknod.
The partition geometry is fixed to the64MiB fixture.

`validate-pair.py CANDIDATE --linux-config RESOLVED_CONFIG --ledger N` implements
the two-boot sequence: fresh backing/token, write+fsync+read-only remount+
guest reset; offline snapshot/fsck; fresh outer QEMU/L1/L2 with the same backing
inode; read+compare+reset; offline verification and ordered evidence analysis.
It never restores RAM or recreates the backing file between starts. `N` is
the prior conservative upper bound of architectural L1 WBINVD observations,
not a physical cache-operation count. The accepted prior accounting is in
`cache-ledger.json`, including its one historical lost-capture uncertainty.

`prepare-native.py` admits a specific new PIIX3 PIO device and backing file,
while retaining the older paused-QMP graph for every other device. Host/module
and executable hashes must still match the pinned fixture. Runs are one-shot;
use a new candidate after a failed attempt. The supplied native paths are
host-specific and require the recorded KVM/firmware/toolchain setup.

Read-only post-verification:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/analyze_persistent_disk.py CANDIDATE
sh krnl/tests/virtio_rw.sh
sh krnl/tests/pic_irq5.sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools \
  -p test_analyze_persistent_disk.py
```
