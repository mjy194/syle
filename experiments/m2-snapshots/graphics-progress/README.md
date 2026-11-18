# Graphics checkpoint

Acceptance: `efi/build/graphics-no-ps2-20261118/graphics-analysis.json`.
Implementation and native evidence: `docs/firmware-graphics.md`.

The source generator verifies the frozen Ring3 lifecycle manifest, copies its
complete source, then adds only the ramfb/DMA and scanout profile. Use a fresh
candidate after each attempted runtime. Keep old accepted sources/media intact.

```sh
python3 experiments/m2-snapshots/graphics-progress/build-linux.py \
  --output NEW_LINUX_DIRECTORY
python3 experiments/m2-snapshots/graphics-progress/prepare.py --output CANDIDATE
sh experiments/m2-snapshots/graphics-progress/build.sh \
  CANDIDATE GNU_EFI_PREFIX OVMF_CODE OVMF_VARS
python3 experiments/m2-snapshots/graphics-progress/prepare-disk.py CANDIDATE \
  --kernel NEW_LINUX_DIRECTORY --efi-prefix GNU_EFI_PREFIX
sh boot/tests/kernel_elf.sh "$PWD/CANDIDATE/source/efi/build/krnl-firmware-disk.elf"
python3 experiments/m2-snapshots/graphics-progress/prepare-native.py \
  CANDIDATE --ledger PRIOR_CONSERVATIVE_UPPER_BOUND
python3 CANDIDATE/native/validate_once.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/analyze_graphics_firmware.py CANDIDATE
```

The Linux builder reuses the previously patched/pinned Linux 6.12 source and
copies the old disk-root object directory to a separate output. The resolved
config enables EFI framebuffer/VT and disables SERIO and keyboard/mouse
probing. It adds no initramfs. Optional `--base OBJECT_DIRECTORY` changes only
the starting object/config copy; the accepted second build used the first
graphics object's directory, then disabled PS/2 drivers. The final resolved
config and kernel hashes are in the accepted inputs.

The disk builder starts with the hash-pinned seed disk's ext2 partition,
installs a newly compiled framebuffer helper using debugfs, builds the actual
GOP EFI probe from the candidate source, and makes a new GPT/FAT/ext2 image.
It never mounts or uses a host block device.

Native scripts retain the host-specific prior KVM/module/OVMF pins, tiny/no-Hv
controls and paused-QMP admission. No extra display device or host serial input
is used. During the positive run, QMP captures the UEFI and Linux patterns after
250ms stability. The analyzer verifies actual pixel channels and quadrants,
Linux framebuffer registration, source/media hashes, disk fsck and all previous
lifecycle checks. Use **82** as the next prior ledger upper bound after this
checkpoint, accounting separately for later attempts.

```sh
sh krnl/tests/firmware_ramfb.sh
sh krnl/tests/firmware_ramfb_transaction.sh
sh krnl/tests/firmware_display.sh
sh krnl/tests/firmware_fwcfg.sh
sh krnl/tests/firmware_fwcfg_transaction.sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/test_analyze_graphics_firmware.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/test_analyze_managed_firmware.py \
  CANDIDATE/native/positive-once/serial.log
```

The new sanitizer tests cover DMA bounds/overlap/completion and fatal partial
commits, instruction/page-table protection, host destination bounds, RGB/BGR
conversion, cadence, stale ownership and scanout revoke. The synthetic pixel
tests reject channel/pane swaps, blank and undersized images. Actual GOP,
Linux handoff and release are established only by the native capture.
