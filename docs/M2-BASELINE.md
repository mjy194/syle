# M2 direct-Linux regression baseline

This checkpoint preserves the working direct Linux path while guest firmware,
storage and display are developed separately. It is not final M2 acceptance.
Outer QEMU/OVMF boots Syle; it is not Syle's guest firmware.

## Recorded checkpoint, 2026-11-13

Archbook nested KVM passed the same-handle STOP/START probe after the probe was
made freestanding across the host compilers and its result markers were given
explicit line boundaries. The guest preserved its shell variable and PID,
emitted independent ARMED/RESUME_OK lines, continued for 256 slices and exited
management with status 0. The earlier marker-prefixed run is diagnostic only.

The new independent initramfs fixture passed three fresh ESP/VARS KVM runs: the static
ELF verified arguments and file operations, its explicit exit 37 was checked,
BusyBox sleep returned, and BASELINE_OK plus management status 0 were observed.
The runs completed in approximately 41.68, 41.69 and 41.48 host seconds. This
does not establish real-time sleep accuracy or long-duration stability.
An additional external-terminal run observed `/proc/uptime` 0.36 then 1.75
around `sleep 1` (guest delta 1.39); the host-side command phase took 30.84
seconds including input pacing. After five host seconds idle, an independently
entered command returned IDLE_OK and Ctrl-] cleaned up with status 0. This proves
return/monotonic progress and idle input in that run, not wall-clock accuracy.

The historical kernel contains the version string Linux 6.12.0-syle-m2,
gcc Debian 14.2.0-19 and GNU ld 2.44, with a fixed build timestamp. The nearby
project material and decompressed image did not supply a Linux .config or
embedded IKCONFIG during this check. These toolchain clues do not reconstruct
the original configuration; source-level kernel reproducibility remains open.

## Frozen historical input

The default managed builds retain the original `bzImage` and `initramfs.cpio`
hashes in `efi/managed-guest.mk`. Preserve these bytes and their provenance.
Their source/config reconstruction is a separate deliverable: a matching hash
does not imply a reproducible source build has been documented.

For an isolated run using those inputs:

```
python3 tools/managed_run.py --inputs /absolute/fixed-inputs \
  --ovmf-code /absolute/OVMF_CODE.fd --ovmf-vars /absolute/OVMF_VARS.fd \
  --run-dir /absolute/new-run --seconds 180 --run
```

The matching GNU EFI headers/libraries and a QEMU with usable nested KVM must
already be available. `EFI_PREFIX` can select an unpacked GNU EFI installation.
The script never installs packages, changes KVM permissions or falls back to
TCG. It copies tracked and untracked non-ignored source into the new run directory
and rebuilds that snapshot, rejects source symlinks, then checks that the snapshot
did not change during the build. It captures build/source/artifact hashes,
creates a fresh private ESP/VARS and uses a local serial socket with no
network. Omit `--run` to prepare only. A run directory must be new; historical
logs are never overwritten. The run directory must be outside the checkout.
The supplied timeout includes waiting for a client. Freeze edits while taking the
snapshot; the manifest identifies the copied bytes rather than claiming they
were an atomic Git revision. Serial output is saved in `serial.log`.

In another terminal, connect with `tools/serial_terminal.py` and record the
session. A QEMU process exit or timeout alone is not acceptance. Require actual
guest response lines and `SYLE.TERMINAL detached; guest cleaned up` / successful
management exit. Do not expose the socket to untrusted users.

## Separate acceptance cases

1. Repeat fresh runs three times, independently verify program output and
   cleanup. Three runtime logs now exist for the frozen baseline candidate;
   repeat them after relevant code/input changes, and separately verify clean
   source rebuild reproducibility rather than inferring it from runtime repeats.
2. Use `managed-resume-probe` to verify shell variable and PID retention across
   STOP/START of the same handle. See `user/mgmt/RESUME_PROBE.md`.
3. Use the separately versioned `guest/linux-baseline` fixture to execute an
   independent static ELF, verify arguments, file syscalls and exit status 37.
   Never silently substitute its new initramfs for the historical input.
4. Verify sleep return, idle input, multiple commands and timer monotonicity;
   then increase the bounded run duration for stability tests. The present
   exit-driven virtual time is not an exact wall-clock contract.
5. Exercise RAM ownership/alias/permission boundaries, Ring3 negative tests,
   failed teardown retention and declared CPU/MSR/FX-state isolation. A shell
   success log does not replace these cases.

The default guest page-fault policy stays native. Keep explicit diagnostic PF
interception separate. Guest EPT RWX RAM is not by itself a defect: guest page
tables implement its internal permissions; EPT must protect host ownership and
the defined guest physical address space.

## Evidence requirements

Record source commit **and uncommitted source hashes**, input and artifact
hashes, target/kernel/manager pairing, build command, environment, serial input
and output, exact acceptance assertions, process exit and cleanup. Label host
tests, nested-KVM runtime and physical bare-metal tests separately. Do not infer
firmware, storage, display or long-duration acceptance from direct Linux boot.
