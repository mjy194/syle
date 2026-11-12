#ifndef SYLE_IOAPIC_H
#define SYLE_IOAPIC_H
#include "device_bus.h"
#define IOAPIC_PINS 24
typedef bool (*ioapic_enqueue_fn)(void *context, uint8_t vector);
struct ioapic_pin { uint32_t low; bool asserted, remote_irr; };
struct ioapic {
    uint8_t selector;
    struct ioapic_pin pins[IOAPIC_PINS];
    ioapic_enqueue_fn enqueue;
    void *context;
};
/* Single destination CPU0, fixed delivery, active-high. No MSI, polarity,
 * lowest-priority, logical destination, arbitration or multicore routing. */
void ioapic_init(struct ioapic *a, ioapic_enqueue_fn enqueue, void *context);
enum device_status ioapic_mmio(void *context,uint64_t offset,unsigned width,
    bool write,uint32_t *value);
/* A failed enqueue retains the level line but never sets remote IRR.
 * Edge events are pulses: masked/failed rising edges are not latched. */
bool ioapic_set_irq(struct ioapic *a,unsigned pin,bool asserted);
bool ioapic_retry(struct ioapic *a,unsigned pin);
bool ioapic_eoi(struct ioapic *a,uint8_t vector);
#endif
