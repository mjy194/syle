#ifndef SYLE_PCI_PROBE_NODMA_H
#define SYLE_PCI_PROBE_NODMA_H
#include "firmware_pci_bus.h"
/* Enumeration-only initializer. Every guest RAM admission/read/write refuses.
 * Not linked into default or any current frozen guest. Dummy capacity is only
 * an enumeration placeholder; no block request can return its contents. */
bool vfb_init_nodma_probe(struct vfb*,uint64_t,uint64_t,uint64_t);
#endif
