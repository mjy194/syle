#ifndef SYLE_FIRMWARE_CONSOLE_H
#define SYLE_FIRMWARE_CONSOLE_H
#include "linux_uart.h"
#include <stddef.h>
/* Serialized stopped vCPU. Synthetic PCI 00:02.0, IO BAR0 only, no DMA.
 * Caller validates mode/event state and actual instruction bytes. */
struct fcon_state { struct linux_uart uart; uint32_t bar; uint8_t command; bool probe,failed; uint64_t reads,writes,received; };
bool fcon_config(struct fcon_state*,unsigned offset,unsigned width,bool input,uint64_t*value);
bool fcon_port(const struct fcon_state*,uint16_t port);
/* Transactional IO: UART/GPR/output publish only after RIP commit. Failed
 * VMWRITE may be partial and permanently forbids reuse. TX drain is immediate;
 * polling only: IER nonzero is rejected, no UART IRQ is silently discarded. */
bool fcon_io(struct fcon_state*,uint64_t g[15],uint64_t rip,uint64_t q,unsigned len,
 const uint8_t*bytes,size_t available,void*,bool(*write)(void*,uint32_t,uint64_t),bool*emit,uint8_t*byte);
bool fcon_receive(struct fcon_state*,uint8_t);
#endif
