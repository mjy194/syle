#ifndef SYLE_GUEST_UART_H
#define SYLE_GUEST_UART_H
#include <stdbool.h>
#include <stdint.h>

/* Minimal virtual COM1 register model, independent of guest OS and boot ABI.
 * Polling TX only: no RX, IRQ, timing, baud rate simulation or FIFO backlog.
 * This is not a complete 16550. The caller resolves the COM1 base address and
 * supplies register offsets 0..7. No host port I/O is performed.
 *
 * LCR supports the unconfigured reset value 0 and 8N1 value 3, with optional
 * DLAB (0x80). TX is accepted only in 8N1 mode with DLAB clear. DLL/DLM are
 * readable/writable when DLAB is set; their values do not affect TX timing.
 * IER must remain zero. MCR supports DTR/RTS/OUT1/OUT2 (bits 0..3), recorded
 * without external line effects; loopback and all other bits are rejected.
 * FCR supports enable, RX/TX clear and trigger bits (0xc7). Clear bits are
 * write-only pulses and do not persist. Enable and trigger bits persist;
 * IIR reads 0x01, or 0xc1 when FIFO is enabled. There is no queued data.
 * LSR reads 0x60 (THRE|TEMT), RBR and MSR read zero. Writes to LSR/MSR are
 * rejected. SCR is ordinary byte storage. No interrupts are generated.
 */
struct guest_uart {
    uint8_t dll, dlm, lcr, mcr, fcr, scr;
};
/* A null argument is a harmless no-op. */
void guest_uart_init(struct guest_uart *uart);
/* All pointers must be valid, distinct output objects outside *uart.
 * width is in bytes and must be 1; writes require *value <= 255.
 * On success, reads replace *value, writes leave *value unchanged; *emit is
 * true only for a transmitted byte (including NUL), and *byte is that byte
 * or zero if nothing was emitted. Rejecting an argument, register access or
 * mode leaves the entire model and every output unchanged. The caller must
 * consume an emitted byte once, after success; no callbacks occur here.
 */
bool guest_uart_access(struct guest_uart *uart, unsigned offset, unsigned width,
                       bool write, uint32_t *value, bool *emit, uint8_t *byte);
#endif
