# PM timer delay: isolated measurement profile

This experiment starts from the manifest-verified PAUSE runtime, not the
checkout's older quiet platform. `prepare.py` copies only manifested sources
into a new directory and applies anchored diagnostic changes. Existing source
trees and normal firmware targets are not updated. Build the generated tree
only with its dedicated `firmware-pm-delay` target.

```sh
python3 experiments/m2-snapshots/pm-delay-progress/prepare.py \
  --base efi/build/firmware-pause-20261114._jzfs3h2 \
  --output /tmp/syle-pm-countdown \
  --stack-frames 8 --shell-delay-probe

make -C /tmp/syle-pm-countdown/source/efi \
  -f Makefile -f firmware-probe.mk -f firmware-ovmf-observe.mk \
  -f firmware-ovmf-clock.mk -f firmware-ovmf-apic.mk \
  -f firmware-ovmf-cache-kvm.mk -f firmware-ovmf-irq.mk \
  -f firmware-code-rom.mk -f firmware-pause.mk \
  -f firmware-pm-delay.mk -f firmware-ovmf-quiet.mk \
  firmware-pm-delay PM_DELAY_CAP=131072 \
  EFI_PREFIX=/path/to/gnu-efi/usr \
  OVMF_CODE=/path/to/OVMF_CODE_4M.fd OVMF_VARS=/path/to/OVMF_VARS_4M.fd
```

The admitted exit caps are 6144 (measurement), 32768 (one-second trial), and
131072 (five-second Shell countdown). The 30-second reference deadline,
per-entry VMX slice, 2 MiB trace capacity, and instruction/device/IRQ admission
remain unchanged. Repeated known loop logs are sampled every 256 exits;
failures, target changes, target crossings and IRQ receipts remain recorded.
These are selected records, not a complete log of every exit. Each actual
PM read, PAUSE completion, clock sample and admission check still executes.

The optional Shell probe checks the observed caller and actual call-site
bytes before reading the configured countdown and saved caller RSI through
the existing owned-RAM reader. It never changes guest memory, page tables,
registers, ticks, or execution admission. Its stack evidence is diagnostic.
The eight-frame profile also adapts and runs the existing stack boundary test.

Offline analysis verifies the pinned firmware and extracted Metronome/Shell
hashes. Use `--sampled` only for the new explicit snapshot format:

```sh
python3 tools/analyze_firmware_pm_delay.py \
  --code /path/to/OVMF_CODE_4M.fd --serial /path/to/serial.log --sampled
python3 tools/test_analyze_firmware_pm_delay.py
```

Native evidence directories contain source/media manifests, exact ELF files,
loader checks, fresh tiny/no-Hyper-V control runs, and an exclusive
`positive_once.py` runner with QMP graph/module admission and cleanup.
Attempt markers prevent accidental reruns of an existing evidence directory.
The tiny control reuses the previously verified tiny ELF with fresh media;
the no-Hyper-V control uses each new candidate ELF.

Results and their limits are recorded in `docs/firmware-pm-delay.md`.
