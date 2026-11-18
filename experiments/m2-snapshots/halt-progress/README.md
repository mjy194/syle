# Firmware HLT idle/wake probe

`prepare.py` copies the manifest-verified final PM-countdown runtime, adds the
HLT module/adapter/tests/make target, and patches only the observer. Existing
snapshots are never modified. The native base contains PM1/DE/compact-trace
overlays absent from the ordinary checkout; do not replace it with the older
checkout observer.

```sh
python3 experiments/m2-snapshots/halt-progress/prepare.py \
  --base efi/build/firmware-pm-delay-20261114-final \
  --output /tmp/syle-firmware-halt

make -C /tmp/syle-firmware-halt/source/efi \
  -f Makefile -f firmware-probe.mk -f firmware-ovmf-observe.mk \
  -f firmware-ovmf-clock.mk -f firmware-ovmf-apic.mk \
  -f firmware-ovmf-cache-kvm.mk -f firmware-ovmf-irq.mk \
  -f firmware-code-rom.mk -f firmware-pause.mk -f firmware-pm-delay.mk \
  -f firmware-halt.mk -f firmware-halt-runtime.mk -f firmware-ovmf-quiet.mk \
  firmware-halt EFI_PREFIX=/path/to/gnu-efi/usr \
  OVMF_CODE=/path/to/OVMF_CODE_4M.fd OVMF_VARS=/path/to/OVMF_VARS_4M.fd
```

The generated runtime makefile retains the 131072-exit, 30-second reference,
2 MiB trace and existing VMX-slice budgets. Its additional probe condition
stops after four HLT completions, at least one uninjected idle slice, three
verified IRQ wakes and three guest EOIs. It reports only repeated wait/wake
acceptance. The checkout's `firmware-halt` target does not enable this probe
condition and does not bring in the external platform overlays.

```sh
sh krnl/tests/firmware_halt.sh
python3 tools/analyze_firmware_halt.py /path/to/positive-once/serial.log
python3 tools/test_analyze_firmware_halt.py /path/to/positive-once/serial.log
```

The host suite exercises the real owned-memory fetcher and existing timer IRQ
session/receipt bridge. Its simulated hardware transitions are not native
acceptance. Native validation uses fresh media, tiny and no-Hyper-V controls,
the existing paused-QMP device/module admission, one exclusive positive
attempt marker and bounded cleanup. See `docs/firmware-halt.md` for results.
