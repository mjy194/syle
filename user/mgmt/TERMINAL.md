# Opt-in serial terminal

This is the smallest human input path for the fixed single-vCPU Linux image:

```
local terminal -> QEMU COM1 / host UART -> host input syscall
 -> Ring3 manager -> guest RX-space query + SEND -> guest UART IRQ4
 -> Linux ttyS0 / shell -> guest TX queue -> manager output -> host COM1
```

Build from `efi/`:

```
make -f Makefile -f managed-terminal.mk managed-terminal MG_INPUTS=/path/to/inputs
```

Pair `krnl-managed-terminal.elf` and `mgmt-guest-terminal.elf` in a fresh isolated
ESP, using the known fixed kernel/initramfs. The ordinary default and successful
shell-probe builds do not enable host input. The terminal uses native guest PF
handling and PIC04, with its own host runtime/Ring3 object files and no periodic
progress diagnostic profile. It does not change host hardware configuration.

## Connecting a person

In the existing isolated/no-network QEMU invocation, replace the output-only
`-serial file:...` with a local Unix serial socket and keep the monitor separate:

```
-chardev socket,id=terminal,path=/absolute/run/console.sock,server=on,wait=on \
-serial chardev:terminal -monitor none
```

Then, from a real terminal on the same computer (including an SSH terminal):

```
python3 tools/serial_terminal.py /absolute/run/console.sock
```

This client has no TCP/network listener. It temporarily puts only its current
terminal into raw mode and restores it on exit. `Ctrl-\\` disconnects the client;
`Ctrl-]` is passed to Syle and requests STOP/DESTROY. A client disconnect alone
does not stop the VM: retain the outer QEMU timeout or stop it through its owning
process. Do not expose the console socket to untrusted users; it controls the
guest shell. Wait for the Linux `# ` prompt before typing.

## ABI and flow control

The explicit terminal kernel adds `SYLE_HOST_CONSOLE_RECV=4` with both arguments
zero. It polls the already-initialized COM1 line status once, consumes at most
one byte, and returns 0..255, EMPTY(-1), UNAVAILABLE(-2), LINE_ERROR(-3), or
INVALID(-4). Empty reads do not consume data. Overrun/parity/framing/break errors
stop the frontend rather than silently discarding data. No user pointers,
configurable port addresses or raw I/O access are exposed. In ordinary kernels
syscall 4 remains unsupported.

The terminal waits for this fixed initramfs's `SYLE_LINUX_USERSPACE_READY` and a
root `# ` prompt. Each iteration executes one guest slice, drains at most 32 TX
bytes, then queries `SYLE_GUEST_CONSOLE_SPACE`. Only with positive capacity does
it consume one host byte and immediately SEND it, without a guest STEP between
query and SEND. This relies on the existing single-BSP/non-reentrant controller.
No bytes are read when capacity is zero or temporarily busy. Any unexpected
SEND error ends the run without retry because the legacy error does not fully
encode commit state. Terminal flow control does not inspect character echoes or
search for success strings, so asynchronous guest logs are not misread as input
acknowledgements.

## Scope and limits

- This is a text serial console, not USB/PS2 keyboard support or a graphical TTY.
- Host boot diagnostics and Linux output share COM1; arbitrary binary/NUL output
  is not supported by the existing text-output syscall.
- No host-input IRQ driver, unbounded host buffering or arbitrary-size paste
  guarantee is claimed. Hardware errors are explicit; type at human speed.
- The frontend stops after 524288 slices or Ctrl-], with STOP/DESTROY checked.
  A clean detach/budget exit is transport cleanup, not proof a shell command ran.
- The fixed-command probe remains the separate automated command acceptance test.

Tests:

```
sh user/mgmt/tests/guest_terminal.sh
sh krnl/tests/host_uart_input.sh
EFI_PREFIX=/path/to/gnu-efi/usr sh krnl/tests/host_console.sh
```

These are mock/host tests; real acceptance requires independently supplied input
through the host serial endpoint, correct command output, and safe cleanup.
