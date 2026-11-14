# PIC04 UART RX timeout clock

`lpd_tick` now advances UART RX age alongside PIT, inside the same staged domain
transaction. It uses the existing virtual PIT frequency (1193182 Hz), UART baud
base (115200 / DLL:DLM), and LCR frame length: start bit, 5–8 data bits, optional
parity, and 1 / 1.5 / 2 stop bits. Half-bit arithmetic preserves the 5-bit,
1.5-stop case. Divisor zero has no defined baud in this model and does not age RX.

Elapsed PIT ticks are converted to character times with a retained fractional
numerator. A threshold comparison caps advancement at the model's four-character
RX timeout before multiplying the elapsed ticks, avoiding overflow even for
UINT64_MAX ticks. The accumulated fraction stays below one character period;
no floating point or 128-bit division helpers are needed in the kernel.

RX activity (including overrun, matching the existing UART model), a nonempty RBR
read, loopback TX-to-RX, RX FIFO clear, and changes to divisor/frame/FIFO
configuration reset the fractional age. Configuration changes also restart whole
character age; DLAB-only changes do not. Empty RX has no pending timeout. IRQ4
is synchronized in the staged copy; failures publish neither domain nor age.

This is an RX timeout clock bridge, not baud-rate paced transmission or a complete
16550 simulator. It advances only on virtual device clock ticks, not wall time.
The idle planner may batch time past a timeout, so delivery can be late relative
to its threshold, never early. A separate UART deadline-aware idle planner would
be needed for exact virtual scheduling latency.

Validation:

- `sh krnl/tests/linux_pic04_uart_clock.sh`: all supported LCR frame combinations
  at divisors 1, 12, 65535; exact and fractional boundaries; below-trigger IRQ4;
  RX/RBR/config resets; FIFO clear; huge and backward timestamps; failed VMWRITE.
- `sh krnl/tests/linux_pic04_integration.sh`: existing full domain/dispatch tests.

KVM baseline observation (2026-11-13, before clock bridge): after BusyBox's shell
prompt, one probe byte was queued (`rc=1`) but `rx_idle=0` persisted. FCR was
0x81 (FIFO enabled, trigger 8), IER=5, MCR=0xb, LCR=0x13, divisor=1; IRQ4 stayed
low. This distinguishes a missing RX timeout from a failure to send the byte.
A later sample had six queued bytes and still zero RX age. The probe can match
individual echo characters in asynchronous console messages, so the observation
is not a claim that it remained on the first byte forever. The host tests above
do not by themselves establish a successful guest command.
