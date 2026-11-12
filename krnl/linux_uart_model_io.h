#ifndef SYLE_LINUX_UART_MODEL_IO_H
#define SYLE_LINUX_UART_MODEL_IO_H
#include "linux_uart.h"
/* Byte DX-only CPL0 real reason30 len1. Caller rejects pending/vectoring.
 * All device read-clear/queue effects and AL commit only after VMWRITE RIP.
 * Failure leaves UART/GPR/irq output unchanged; VMCS failure forbids resume.
 * Success returns logical IRQ level; scheduler publishes afterward, outside
 * this transaction. Sink failure may retain external level, use sink retry. */
bool lumio_apply(struct linux_uart*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t qualification,unsigned len,void*,bool(*write)(void*,uint32_t,uint64_t),bool*irq_level);
#endif
