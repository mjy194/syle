# Same guest STOP/START acceptance probe

Independent opt-in target; existing shell/terminal probes and kernel semantics
are unchanged. Build with the frozen Linux inputs:

```
make -C efi -f Makefile -f managed-resume-probe.mk managed-resume-probe MG_INPUTS=/absolute/inputs
```

Pair `krnl-managed-pic04.elf` and `mgmt-guest-resume-probe.elf` in a new isolated
ESP. This build does not launch QEMU.

The manager creates exactly one guest handle, waits for the fixed initramfs
ready marker and root shell prompt, then asks the shell to set a variable and
save its PID. Only after receiving a separate `SYLE_RESUME_ARMED` line does it
STOP and START the **same handle**, without DESTROY or CREATE. The second command
checks the variable and PID and emits a separate `SYLE_RESUME_OK` line only if
both are retained. Echo text contains split marker strings, so command echo
alone cannot satisfy either check. The manager then executes 256 more successful
slices before STOP/DESTROY. Only the full sequence and cleanup returns zero.

Commands use the existing CONSOLE_SPACE capacity query, not character echo as
flow control. There is no guest STEP between the capacity query and one SEND;
unexpected errors terminate without retrying an ambiguously committed byte.
The overall run is bounded to 524288 slices.

This demonstrates state retained in the shell process across a management
pause/resume. It does not prove every register, memory page, timing source or
failure path is restored correctly, nor that the guest survives reboot. A
non-matching response or failed command cannot be counted as acceptance.

Host tests:

```
sh user/mgmt/tests/guest_resume_probe.sh
```

They cover retained state, reset/lost state, missing response, resume failure,
capacity stalls, SEND/output failures and cleanup errors. Mock success and an
ELF build are not Linux/KVM acceptance. Record runtime hashes and the complete
serial log separately.
