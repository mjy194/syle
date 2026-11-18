# Single-vCPU UEFI disk guest

Run from the checkout root on the Intel nested-KVM host used for the graphics checkpoint. The profile uses the accepted 64 MiB GPT disk with an EFI system partition, an EFI Linux kernel and an ext2 root. Paths beginning with NEW must be unused. The build stages the checkout and the profile-specific platform sources together under the output directory.

```sh
python3 m2/build.py NEW_BUILD GNU_EFI_PREFIX GUEST_OVMF_CODE GUEST_OVMF_VARS
python3 m2/run.py NEW_BUILD NEW_RUN --seed SEED_DISK --disk WRITABLE_DISK --vars HOST_OVMF_VARS
```

The runner copies the seed when the writable disk is first created. Reuse the writable disk with a fresh run directory for subsequent launches. The build uses the guest firmware passed on its command line. The runner uses the host code image at `/usr/share/edk2/x64/OVMF_CODE.4m.fd` and makes a private copy of the supplied host variables. Add `--display gtk` for a host framebuffer window. Management and shell input use the serial terminal.

| Key | Operation |
| --- | --- |
| c | Create a fresh guest using the backing disk |
| s | Start or resume the guest |
| a | Forward serial input to the guest |
| Ctrl-] | Return to management while the guest continues running |
| p | Pause the guest and retain its execution state |
| d | Destroy the guest and release its resources |
| q | Destroy any remaining guest and exit management |

For repeatable acceptance, add `--check` to the run command. This writes a fresh token to `/persist/probe.txt` on the writable disk. It checks the same shell after pause/resume and 130 seconds of idle running, remounts the root read-only before destroying the guest, checks the token after a fresh boot, and checks resource release for both generations. Each run stores its serial transcript and QEMU command in its run directory. `--idle SECONDS` selects another observation interval. `--socket` exposes the same serial stream to a separate client instead.

## Acceptance on 2026-11-18

Two independent QEMU/KVM launches used the same writable backing file. Each launch created two guest generations and ended with management exit status zero. Evidence is under `efi/build/m2-run-v1/serial.log` (130 seconds of running idle) and `efi/build/m2-run-v2/serial.log` (5 seconds, randomized disk token). All four releases returned to 262 allocated pages, with VMX inactive and the EPT and disk adapter released. A read-only offline e2fsck of the second GPT partition after both launches returned zero. The built kernel passed the ELF loader contract. Display pixel verification for the base profile is recorded in [the graphics checkpoint](../docs/firmware-graphics.md).

Each run recorded two L1 WBINVD attempts. With the preceding graphics checkpoint, the cumulative ledger is 85 observed plus one historical capture uncertainty, giving a conservative upper bound of 86. The idle interval exercised a running guest. The host clock's maximum sampling gap still bounds pause/resume.

For the optional single-vCPU nested KVM profile, add
`--nested` to the build command. The Ring3 menu then offers `n` for nested
guest creation alongside `c` for ordinary creation. See the
[L2 Linux direct64 acceptance](../experiments/m4-linux-kvm/README.md#single-vcpu-l2-linux-direct64).
For an interactive L2 boot with the recovered 250 Hz kernel, use the
[shell disk and serial instructions](../experiments/m4-linux-kvm/README.md#interactive-shell-on-the-original-250-hz-kernel).
The [writable L2 disk profile](../experiments/m4-linux-kvm/README.md#l2-linux-with-a-writable-disk-root)
boots Linux from ext2 on virtio-blk and checks a shell-written file across
two L2 cold starts within one L1.
