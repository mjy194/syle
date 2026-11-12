#ifndef SYLE_LINUX_UART_RUNTIME_H
#define SYLE_LINUX_UART_RUNTIME_H
#include "linux_uart_model_io.h"
enum lurt_result {LURT_REJECTED=-1,LURT_COMMITTED=0,LURT_IRQ_PENDING=1};
/* Private caller-owned context, fixed single BSP, nonconcurrent. Callback
 * must synchronize latest logical IRQ4 level AND retry pending delivery.
 * If sink is IOAPIC, set_irq alone may not retry; adapter owns ioapic_retry.
 * False may already retain external level. Never repeat a committed operation.
 * No user pointers, host ports, clock reads or external registration. */
struct linux_uart_runtime {struct linux_uart uart;void*irq_context;bool(*sync_irq)(void*,unsigned,bool);bool irq_dirty,busy,ready;};
/* Initialize only a fresh or fully quiescent context, never from a callback.
 * busy guards ordinary reentry; it is not a boundary against arbitrary
 * trusted kernel callers overwriting/reinitializing this object. */
bool lurt_init(struct linux_uart_runtime*,void*,bool(*sync_irq)(void*,unsigned,bool));
enum lurt_result lurt_retry_irq(struct linux_uart_runtime*);
/* On nonnegative result device effects occurred; pending means IRQ sync failed.
 * receive full sets accepted=false and OE, not transport failure.
 * TX empty sets available=false, byte=0. Consumer handles returned byte once.
 * REJECTED guestIO forbids resume (VMWRITE may have failed). */
enum lurt_result lurt_receive(struct linux_uart_runtime*,uint8_t,bool*accepted);
enum lurt_result lurt_tick(struct linux_uart_runtime*,unsigned character_times);
enum lurt_result lurt_modem(struct linux_uart_runtime*,uint8_t high_nibble);
enum lurt_result lurt_pop_tx(struct linux_uart_runtime*,bool*available,uint8_t*byte);
enum lurt_result lurt_guest_io(struct linux_uart_runtime*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t qualification,unsigned len,void*,bool(*vmwrite)(void*,uint32_t,uint64_t));
#endif
