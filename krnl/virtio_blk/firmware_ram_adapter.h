#ifndef SYLE_VIRTIO_FIRMWARE_RAM_ADAPTER_H
#define SYLE_VIRTIO_FIRMWARE_RAM_ADAPTER_H
#include "transport.h"
#include "../firmware_memory.h"
/* Native/host integration seam. Controller must hold
 * exclusive stopped-vCPU ownership, freeze fwm+callbacks+allocator backing and
 * exclude concurrent DMA/release throughout device access and instruction commit.
 * Immutable disk/metadata/host buffers must be disjoint from ALL guest mappings.
 * These are trusted integration preconditions, not established by owned_hash.
 * Publication executes a release fence before used.idx is stored. */
struct vfr { struct firmware_memory *memory; bool active; unsigned publications; };
bool vfr_bind(struct vfr *,struct firmware_memory *,struct vro_io *);
/* Call before any firmware release, or failed/uncertain outer I/O commit.
 * Never reactivate the same device after abort, even if a store succeeded. */
void vfr_abort(struct vfr *);
#endif
