# Ring3-managed firmware checkpoint

Accepted native evidence: `efi/build/managed-firmware-remount-20261118/analysis.json`.
See `docs/firmware-managed.md` for scope, ownership, counters and limitations.
Use a new candidate for each attempted runtime; never alter attempted sources
or media. This profile must use the frozen persistent runtime's platform
changes, rather than the older ordinary checkout observer.

```sh
python3 experiments/m2-snapshots/managed-firmware/prepare.py --output CANDIDATE
sh experiments/m2-snapshots/managed-firmware/build.sh \
  CANDIDATE GNU_EFI_PREFIX OVMF_CODE OVMF_VARS
sh boot/tests/kernel_elf.sh "$PWD/CANDIDATE/source/efi/build/krnl-firmware-disk.elf"
python3 experiments/m2-snapshots/managed-firmware/prepare-native.py \
  CANDIDATE --ledger PRIOR_CONSERVATIVE_UPPER_BOUND
python3 CANDIDATE/native/validate_once.py
PYTHONDONTWRITEBYTECODE=1 python3 tools/analyze_managed_firmware.py CANDIDATE
PYTHONDONTWRITEBYTECODE=1 python3 tools/test_analyze_managed_firmware.py \
  CANDIDATE/native/positive-once/serial.log
```

The native scripts are host-specific KVM fixtures. They reuse the exact earlier
host/module/firmware pins, tiny/no-Hv controls, paused admission, and fixed PIO
backing graph. `prepare-native.py` copies and hash-verifies the previous seed
disk; the manager creates all test data. Host serial input is empty. Each
native validator/positive runner is one-shot. The validator preserves control
and positive results; the separate analyzer makes the final lifecycle and
offline filesystem acceptance decision. Use prior upper bound **79** after
this checkpoint; count any later attempts before selecting a new bound.

The lifecycle manager is embedded read-only in the kernel and loaded through
the production Ring3 ELF loader. The disk payload keeps its original ABI.
The generated engine preserves every old execution transaction but returns
after at most 64 entries. STOP/START keeps CPU/RAM/devices; DESTROY frees and
then allows fresh initialization and a new handle. There are no new userspace
syscalls or host-address/image parameters.

```sh
sh user/mgmt/tests/firmware_lifecycle.sh
sh krnl/tests/guest_control.sh
sh krnl/tests/managed_host_context.sh
sh krnl/tests/firmware_memory.sh
sh krnl/tests/linux_vcpu_host.sh
sh krnl/tests/virtio_rw.sh
sh krnl/tests/pic_irq5.sh
```

The sanitizer tests cover lifecycle response integrity, stopped-state checks,
retained state, recreate/disk failures, cleanup errors, controller ownership,
quarantine and partial VMX/memory release. Actual VMX, Linux command execution
and complete page balance are established by the native evidence above.
