#ifndef SYLE_LINUX_UART_H
#define SYLE_LINUX_UART_H
#include <stdbool.h>
#include <stdint.h>
#define LUART_CAPACITY 16u
/* Independent bounded polling/IRQ-level model, not a complete 16550.
 * Byte registers accept non-break LCR configuration; TX requires effective 8N1.
 * DLAB maps DLL/DLM only; no EFR, baud timing or break/parity
 * simulation. IER bits0..3, OUT2 gate, modem deltas and loopback supported.
 * FIFO trigger1/4/8/14; timeout requires explicit character-time ticks.
 * TX completion occurs on explicit drain, not host port writes.
 * IRQ is only an abstract level. No guest PIC/APIC injection is wired. */
struct linux_uart {uint8_t dll,dlm,lcr,mcr,ier,fcr,scr;uint8_t rx[16],tx[16];unsigned rh,rc,th,tc;bool overrun,tx_pending;uint8_t modem_external,modem,modem_delta;unsigned rx_idle;};
void linux_uart_init(struct linux_uart*);
/* False rejects without mutation. Full RX accepts no byte but records OE,
 * returning true: producer can inspect accepted=false and LSR reports loss. */
bool linux_uart_receive(struct linux_uart*,uint8_t,bool*accepted);
/* High nibble CTS/DSR/RI/DCD; low bits must be zero. */
bool linux_uart_modem_input(struct linux_uart*,uint8_t);
/* Caller supplies elapsed character times, not wall milliseconds. */
bool linux_uart_tick(struct linux_uart*,unsigned character_times);
bool linux_uart_drain(struct linux_uart*,uint8_t*);
bool linux_uart_access(struct linux_uart*,unsigned reg,bool write,uint8_t*);
bool linux_uart_irq_level(const struct linux_uart*);
/* Explicit bridge only; callback must set an idempotent abstract IRQ level.
 * No registration/persistent callback retained. Failure changes no UART model,
 * but the external sink may already retain the new level; its retry semantics
 * belong to the sink. IOAPIC bridge uses ioapic_set_irq(pin4, asserted), not
 * electrical-level ioapic_set_level and no active-low inversion. */
bool linux_uart_publish_irq(const struct linux_uart*,void*,bool(*set_level)(void*,bool));
#endif
