#ifndef LINUX_GUEST_IRQ_ADAPTER_H
#define LINUX_GUEST_IRQ_ADAPTER_H
#include <guest_apic.h>
#include <ioapic.h>
enum gia_source { GIA_UART_LEVEL, GIA_LOCAL_EDGE };
struct gia {
 struct guest_apic *apic;struct ioapic *io;
 unsigned char saved[sizeof(struct guest_apic)],saved_io[sizeof(struct ioapic)];
 uint64_t domain,generation,token,receipt_sequence;
 uint8_t vector,receipt_vector;enum gia_source source;
 bool enabled,reserved,quarantined,receipt_pending,delivering;
};
/* Fresh/quiescent only, nonzero globally unique domain, default opt_in=false.
 * No model init/CPUID changes. Caller wires IOAPIC enqueue=gia_enqueue/context=c.
 * All calls single-BSP; all guest register writes must respect gia_exclusive. */
bool gia_init(struct gia *,struct guest_apic *,struct ioapic *,uint64_t,bool);
bool gia_exclusive(const struct gia *);
bool gia_peek(const struct gia *,uint8_t *,enum gia_source *);
bool gia_reserve(struct gia *,uint8_t *,uint64_t *,enum gia_source *);
bool gia_accept(void *,uint64_t,uint8_t); /* after proven receipt+VMCS cleanup */
bool gia_cancel(struct gia *,uint64_t,uint8_t); /* after definite no-entry cleanup */
void gia_quarantine(struct gia *);
bool gia_enqueue(void *,uint8_t); /* IOAPIC callback: UART level only */
bool gia_irq4(void *,unsigned,bool);
bool gia_poll(struct gia *,uint64_t now);
enum device_status gia_ioapic_mmio(struct gia *,uint64_t,unsigned,bool,uint32_t *);
/* Idempotent lams_notify sink, not a fresh EOI instruction. False may have
 * applied the EOI already. Repeat exact domain/sequence/vector only. */
bool gia_notify(void *,uint64_t domain,uint64_t sequence,uint8_t vector);
#endif
