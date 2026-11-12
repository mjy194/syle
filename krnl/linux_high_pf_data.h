#ifndef SYLE_LINUX_HIGH_PF_DATA_H
#define SYLE_LINUX_HIGH_PF_DATA_H
#include <stdint.h>
#include <stdbool.h>
/* Exact observed fixed-image data mapping: NX is required with EFER.NXE.
 * This is not a generic PMD validator or execution-permission grant. */
bool lhpd_metadata(uint64_t counter_pair,uint64_t base,uint64_t flags,uint64_t efer);
#endif
