# Bounded continuous serial-output frontend

Opt-in only; normal management demo and lifecycle smoke are unchanged.

```
make -C efi -f Makefile -f managed-continuous.mk \
  managed-continuous MG_INPUTS=/path/to/fixed-linux-inputs
```

This builds `krnl-managed-pic04.elf` and `mgmt-guest-continuous.elf`. It does not
modify an ESP or start a VM. Place these under the loader's `syle/krnl.elf` and
`syle/mgmt.elf` only in an explicitly selected test ESP. The existing fixed
kernel/initramfs hash checks still apply.

For an explicitly requested historical PF diagnostic investigation, use the
diagnostic PF target described in `docs/linux-cpl3-runtime.md`. Ordinary managed
targets now let guest PF execute natively. For non-PF progress sampling, use `managed-continuous-progress` instead
of `managed-continuous` in the command above. This pairs
`krnl-managed-pic04-progress.elf` with the same management ELF and enables the
failure checkpoint/event diagnostics described in
`docs/linux-cpl3-runtime.md`. Neither target changes the default build.

The manager CREATEs one guest, STARTs it once, then STEP continues its retained
VMCS. It drains at most 32 console bytes after each successful slice. `-SYLE_GUEST_EBUSY` or the existing UART service's legacy `-16`
means an empty queue; other errors stop execution. A total budget of 262144
slices bounds the session below the backend's current 1048576-exit limit.
After the budget or any error it attempts STOP and DESTROY, even if STOP fails.
A failed CREATE invokes owner REAP. The kernel also retains its independent
owner-exit cleanup and quarantine guarantees.

Budget completion is not Linux boot or shell success. There is deliberately no
fabricated keyboard syscall, automatic command injection or shell detection.
This output-only target does not enable a host input read primitive. The separate
`managed-terminal` target now supplies opt-in host serial input and forwards it
through CONSOLE_SEND; see `TERMINAL.md`. Neither is a USB/PS2 keyboard driver.

Host-only checks:

```
sh user/mgmt/tests/guest_continuous.sh
make -C efi -f Makefile -f managed-continuous.mk build/mgmt-guest-continuous.elf
```

The first checks slice counts, bounded output draining, empty queues, create /
start / step / receive / output errors and cleanup failures. Neither check
executes VMX or Linux. Actual Linux runtime evidence remains required.
