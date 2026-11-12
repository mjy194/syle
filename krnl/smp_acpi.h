#ifndef SYLE_SMP_ACPI_H
#define SYLE_SMP_ACPI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Reader must copy only accessible physical bytes, returning false otherwise.
 * Firmware must remain stable during discovery. No returned pointer is retained. */
typedef bool (*smp_acpi_read_fn)(void *ctx, uint64_t pa, void *dst, size_t length);
struct smp_acpi_topology {
    uint64_t lapic_base;
    uint32_t bsp_id, ap_id;
    unsigned enabled_count;
    bool used_xsdt;
};
/* Diagnostic contract: exactly two enabled, unique, xAPIC-compatible CPUs.
 * XSDT is preferred; RSDT fallback only if XSDT address is absent.
 * Invalid preferred tables fail closed. Output is unchanged on failure. */
bool smp_acpi_discover(uint64_t rsdp_pa, uint32_t bsp_id,
                       smp_acpi_read_fn read, void *ctx,
                       struct smp_acpi_topology *out);
/* Explicit single-CPU contract for cache-transition diagnostics. Exactly one
 * enabled record matching BSP; enabled_count=1 and ap_id=0 (unused). */
bool smp_acpi_discover_single(uint64_t rsdp_pa, uint32_t bsp_id,
                              smp_acpi_read_fn read, void *ctx,
                              struct smp_acpi_topology *out);
#endif
