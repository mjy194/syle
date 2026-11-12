#ifndef LINUX_ACPI_TOPOLOGY_H
#define LINUX_ACPI_TOPOLOGY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LAT_GPA UINT64_C(0xe0000)
#define LAT_REGION_SIZE 0x10000u
#define LAT_SIZE 202u
/* Host-only RSDP/XSDT/MADT fragment, NOT complete ACPI firmware.
 * No FADT/DSDT/SCI/AML/timers/EFI. Does not select or enable APIC mode.
 * output must be writable and disjoint from out_size; failure changes neither.
 * Caller reserves the entire firmware region in E820 before guest use. */
bool lat_build(void *output,size_t capacity,uint64_t guest_base,size_t *out_size);
#endif
