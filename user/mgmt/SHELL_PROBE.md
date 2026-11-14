# Bounded fixed-command shell probe

This additive frontend uses the existing guest CONSOLE_SEND/RECV ABI and PIC04
profile. It is a fixed command acceptance test, not a physical keyboard bridge,
a general terminal, or a claim that Linux is already interactive.

Build from `efi/` with the verified fixed input directory:

```
make -f Makefile -f managed-shell-probe.mk managed-shell-probe-progress MG_INPUTS=/path/to/inputs
```

Pair `krnl-managed-pic04-progress.elf` with `mgmt-guest-shell-probe.elf` in a new
isolated ESP. Existing continuous/default targets are unchanged. This target
only builds and does not launch QEMU or alter any host configuration.

The frontend waits for `SYLE_LINUX_USERSPACE_READY`, then a `# ` root prompt.
It sends `echo SYLE_""SHELL_OK` followed by LF, one byte at a time. Before sending
the next non-newline byte it waits for the previous character's tty echo. This
keeps at most one RX byte outstanding and assumes the fixed initramfs's default
TTY echo/root prompt. No echo, a different prompt, or a stalled guest causes a
bounded timeout, not success. SEND errors are terminal and never retried because
the current ABI does not expose sufficiently precise commit/error semantics.

Success requires an exact output line `SYLE_SHELL_OK`, only after the final LF
was accepted, followed by 256 additional successful guest slices, plus successful
STOP and DESTROY. The injected command's quoted empty string ensures its terminal
echo does not contain the success marker as a full line. Output is copied without
modification. Total execution is bounded to 262144 slices and at most 32 TX bytes
are drained per slice. A matching line before command completion is not accepted.

Host policy tests:

```
sh user/mgmt/tests/guest_shell_probe.sh
```

Host tests mock the ABI, including byte echo, SEND rejection, missing readiness,
missing echo, incorrect output, cleanup failures and post-response execution
failure. They are not proof of IRQ4 delivery or a real Linux shell. Acceptance
requires a separately recorded VM run with matching output and manager success.

After termination, `SYLE.PROBE` reports the final readiness/prompt recognition,
accepted SEND byte count, outstanding echo state, exact reply recognition and
last guest/SEND return codes. This distinguishes a parser miss from accepted
input followed by a guest failure before echo. These observations do not change
success criteria and a failed diagnostics write makes the manager exit fail.

Live phase observations are emitted when readiness, prompt, first accepted SEND,
complete command, or reply recognition changes. They are host diagnostics only,
not fed into the guest UART stream or used as acceptance evidence. Thus an outer
QEMU timeout still leaves the important probe progress visible.

During command transmission, the next UART byte must be the pending tty echo;
no unrelated bytes may be skipped to find a coincidental character match.
Unexpected output records `echo_mismatch` with expected/observed bytes and fails
the probe. This is deliberately conservative: asynchronous kernel logs can cause
a probe failure even with a healthy guest. Such failure is not a guest fault and
should be addressed by separating/quieting logs or adding a real RX-credit API,
not by treating arbitrary character matches as proof that the input was consumed.
