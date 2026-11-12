#ifndef SYLE_LINUX_UART_IO_H
#define SYLE_LINUX_UART_IO_H
#include "guest_uart.h"
/* Polling TX-only COM1. No host port access. Only byte IN/OUT via DX,
 * CPL0, real reason30, len1; string/REP/immediate/reserved bits reject.
 * Caller verifies no pending/vectoring. On failure all model/GPR/output
 * objects stay unchanged; caller must not resume if VMCS write failed.
 * Consumer emits returned byte once AFTER success; this is not RX/IRQ support. */
bool luart_apply(struct guest_uart*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,void*,bool(*write)(void*,uint32_t,uint64_t),bool*emit,uint8_t*byte);
#endif
