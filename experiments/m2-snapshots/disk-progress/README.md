# Disk runtime candidate

Prepare a never-reused source directory from the manifest-verified accepted
serial runtime:

```sh
python3 experiments/m2-snapshots/disk-progress/prepare.py \
  --base efi/build/firmware-console-20261117-accepted \
  --output /tmp/syle-disk --linux --linux-console
```

The accepted run is `efi/build/firmware-disk-accepted-20261118`; see
`analysis.json`, `verification.json` and `native/positive-once/console.txt`.
Use `build.sh CANDIDATE GNU_EFI_PREFIX OVMF_CODE OVMF_VARS` to build a prepared
candidate with explicit inputs. It expands the previous console makefile
order, adding `firmware-disk.mk` and
`firmware-disk-runtime.mk` before `firmware-ovmf-quiet.mk`, and target
`firmware-disk`. Omit `firmware-console-runtime.mk` and
`firmware-halt-runtime.mk`, which contain older probe stops. Pinned OVMF and
GNU-EFI locations remain explicit build arguments. Keep the frozen platform
and diagnostic overlays; the older checkout observer is only a build check.

`guest/uefi-disk/build.sh` builds the disk EFI application. Set
`UEFI_DISK_CFLAGS=-DSYLE_DISK_LINUX` to make it LoadImage/StartImage Linux.
The `make-disk.py` builder takes this EFI file and optional `--linux LINUX.EFI`,
and writes a new deterministic 64MiB GPT/FAT32 image. No host mount is used.
Use the separate `build-linux.py` recipe with the hash-pinned source archive
and verified initramfs to build the Linux EFI image.

`prepare-native.py CANDIDATE --ledger N` stages fresh controls and a one-shot
native runner using the established recorded media. Its machine/module pins
are host-specific and must be rechecked if the host changes. It uses the
matching disk EFI loader, and places disk.img only in the outer loader ESP.
The nested guest receives a virtual disk through the C backend, not a QEMU
block-device passthrough. The ordinary checkout/default loader is unchanged.
Run loader and host tests before native validation. Source/media hashes and
one-shot attempt markers must verify before entry. Preserve every failed run;
create a new candidate for semantic changes. Guest phase remains bounded by
131072 exits / 30 reference seconds / 2MiB selected trace / existing VMX slices;
outer process timeout is 180 seconds.


For interactive acceptance pass `--linux-console` to `prepare-native.py` as
well. It sends `uname -s`, `uname -r`, and the unique echo command only after
userspace readiness and the actual `# ` prompt. Then run:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/analyze_firmware_disk.py \
  CANDIDATE/native/positive-once > CANDIDATE/analysis.json
```

The analyzer requires ordered disk/firmware/kernel/userspace markers, exact
result lines distinct from command echo, fresh prompts, independently
recorded input, matching accepted/read UART byte counts and IRQ0/IRQ4
receipts. Missing trace, partial capture, disk failure and process errors
fail acceptance. Its seven negative/positive host checks run with:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools \
  -p test_analyze_firmware_disk.py
```

The final configuration exposes a separate ISA COM1 to Linux, keeps the PCI
firmware UART at00:02.0 and block device at00:03.0, and boots its embedded
initramfs with Linux PCI/ACPI disabled. Linux-mounted persistent disks remain
follow-up work; the accepted storage path here is the OVMF virtio block driver.
