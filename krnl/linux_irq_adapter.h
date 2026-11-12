#ifndef LINUX_IRQ_ADAPTER_H
#define LINUX_IRQ_ADAPTER_H
#include <lapic.h>
#include <ioapic.h>
struct lia {
 struct lapic *apic;
 struct ioapic *io;
 struct lapic saved;
 struct ioapic saved_io;
 uint64_t generation, token;
 uint8_t vector;
 bool enabled, reserved, quarantined;
};
/* Fresh context only. Does not enable APIC or alter devices. opt_in defaults false.
 * Caller must wire IOAPIC enqueue=lia_enqueue, context=this separately. */
bool lia_init(struct lia *,struct lapic *,struct ioapic *,bool opt_in);
bool lia_peek(const struct lia *,uint8_t *);
bool lia_reserve(struct lia *,uint8_t *,uint64_t *);
/* Accept only after lei_clear_delivered succeeded and verified receipt.
 * Callback false leaves LAPIC/token unchanged. */
bool lia_accept(void *,uint64_t,uint8_t);
/* Caller proves no entry and successful VMCS cancel first; IRR never cleared. */
bool lia_cancel(struct lia *,uint64_t,uint8_t);
void lia_quarantine(struct lia *);
/* IRQ4 logical level only: no electrical inversion. False may retain level;
 * caller must retain latest level and retry; reserved blocks all mutation. */
bool lia_irq4(void *,unsigned,bool);
bool lia_enqueue(void *,uint8_t);
/* All IOAPIC accesses, including selector writes, use this reserved gate. */
enum device_status lia_ioapic_mmio(struct lia *,uint64_t,unsigned,bool,uint32_t *);
/* EOI pops once; false after pop means retry IRQ4 delivery, NEVER repeat EOI. */
bool lia_eoi(struct lia *,bool *retired,uint8_t *);
#endif
