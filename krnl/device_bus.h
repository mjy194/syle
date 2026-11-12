#ifndef SYLE_DEVICE_BUS_H
#define SYLE_DEVICE_BUS_H
#include <stdbool.h>
#include <stdint.h>
enum device_status { DEVICE_HANDLED, DEVICE_UNSUPPORTED, DEVICE_FAULT };
typedef enum device_status (*device_access_fn)(void *context, uint64_t offset,
    unsigned width, bool write, uint32_t *value);
#define DEVICE_BUS_REGIONS 8
struct device_region { uint64_t base, size; device_access_fn access; void *context; };
struct device_bus { unsigned count; struct device_region regions[DEVICE_BUS_REGIONS]; };
void device_bus_init(struct device_bus *bus);
bool device_bus_register(struct device_bus *bus, uint64_t base, uint64_t size,
    device_access_fn access, void *context);
/* Diagnostic MMIO bus: only aligned 32-bit accesses contained in one 4K page.
 * UNSUPPORTED means no registered target/unsupported device register;
 * FAULT means malformed access or invalid device value/state. */
enum device_status device_bus_access(struct device_bus *bus, uint64_t address,
    unsigned width, bool write, uint32_t *value);
#endif
