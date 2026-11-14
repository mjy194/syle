#ifndef SYLE_HOST_VIRTIO_PLATFORM_H
#define SYLE_HOST_VIRTIO_PLATFORM_H
#include "transport.h"
#define VRP_REGIONS 8
#define VRP_RESERVED 16
/* Trusted board-supplied allocation records. host backing must be pinned,
 * isolated ordinary RAM, not MMIO; metadata must remain immutable while active.
 * owner is an attestation checked here, NOT an allocator/EPT ownership proof. */
struct vrp_region {uint64_t gpa;size_t size;uint8_t *host;uint64_t owner;bool ram,read,write;};
struct vrp_ports {uint16_t first;uint32_t end;}; /* half-open, end <=65536 */
struct vrp {
 struct vrt dev;
 struct vrp_region regions[VRP_REGIONS];unsigned count;
 struct vrp_ports reserved[VRP_RESERVED];unsigned reserved_count;
 uint16_t bdf;bool active;
 unsigned barriers;
};
/* Host adapter uses a counter barrier only: NOT suitable for actual hardware
 * DMA integration. No heap or page mapping; spans cannot cross region records. */
bool vrp_init(struct vrp *,uint16_t bdf,uint64_t owner,const struct vrp_region *,unsigned,
 const struct vrp_ports *,unsigned,const uint8_t *disk,size_t);
/* 0 not routed, 1 handled, -1 terminal. Bus must translate PCI mechanism to BDF.
 * Caller holds exclusive stopped-guest ownership across these calls and the
 * outer instruction commit. Call abort if that final commit cannot complete. */
int vrp_config(struct vrp *,uint16_t bdf,unsigned off,unsigned width,bool write,uint32_t *);
int vrp_io(struct vrp *,uint16_t port,unsigned width,bool write,uint32_t *);
void vrp_abort(struct vrp *);
#endif
