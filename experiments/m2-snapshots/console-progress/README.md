# Firmware serial console probe

Create an isolated runtime from the manifest-verified HLT base:

```sh
python3 experiments/m2-snapshots/console-progress/prepare.py \
  --base efi/build/firmware-halt-20261114 \
  --output /tmp/syle-console
```

Use the makefile order from the previous HLT build, replacing
`firmware-halt-runtime.mk` with `firmware-console.mk` and
`firmware-console-runtime.mk`, before `firmware-ovmf-quiet.mk`. Build the
`firmware-console` target with the same pinned CODE/VARS and GNU-EFI inputs.
The runtime retains 131072 dispatches, 30 seconds of reference time, the VMX
entry slice and 2MiB compact trace. After UART decode is enabled, routine
observation calls are selected once per 256 dispatches; every actual UART TX
byte is retained independently. Error/terminal records are emitted after
selection is disabled. This sampling is only in the prepared runtime.

The diagnostic probe stops after both commands have arrived, a standalone marker
result and three prompts. The external
analyzer separately requires a standalone marker result and `ver` output.
Normal checkout builds omit this probe stop and the native platform overlays.

```sh
sh krnl/tests/firmware_console.sh
sh krnl/tests/linux_uart.sh
python3 tools/analyze_firmware_console.py /path/to/positive-once
```

Run tiny/no-Hv controls before the positive attempt, using fresh VARS/media,
one vCPU, no network, and paused QMP admission of the same PCI/device graph.
Record current kernel/KVM module hashes and versions; the 2026-11-14 values
are stale after the host upgrade. Use a socket serial backend with a dedicated
continuous reader. Send `ver\r` only on the initial `Shell>` and the echo
command only on the next prompt. The archived native runner records input
separately, is exclusive/one-shot and enforces 180 seconds with cleanup.
