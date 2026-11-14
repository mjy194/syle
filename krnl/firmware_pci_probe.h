#ifndef SYLE_FIRMWARE_PCI_PROBE_H
#define SYLE_FIRMWARE_PCI_PROBE_H
#include <stdbool.h>
#include <stdint.h>
struct fpi_state { uint32_t address; };
/* Synthetic configuration-discovery only. No config/device/DMA passthrough.
 * Return false without state/rax changes for unsupported accesses. */
bool fpi_exit(struct fpi_state*,uint64_t qualification,uint64_t *rax);
bool fpi_io(struct fpi_state*,uint16_t port,unsigned width,bool input,uint64_t *rax);
#endif
