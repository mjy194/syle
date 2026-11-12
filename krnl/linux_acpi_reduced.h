#ifndef LINUX_ACPI_REDUCED_H
#define LINUX_ACPI_REDUCED_H
#include "linux_acpi_topology.h"
#define LAR_SIZE 712u
/* Independent opt-in direct-boot ACPI description, NOT UEFI or final firmware.
 * No sleep/reset/PM/SCI services claimed. Default loader/installer unchanged.
 * Caller supplies disjoint writable output and size; invalid args unchanged. */
bool lar_build(void *,size_t,uint64_t,size_t *);
#endif
