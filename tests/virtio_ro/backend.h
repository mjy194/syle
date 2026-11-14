#ifndef SYLE_HOST_VIRTIO_RO_H
#define SYLE_HOST_VIRTIO_RO_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Isolated prototype. NOT linked into kernel. All callbacks are trusted:
 * admit must prove owned writable/readable guest RAM for the entire span;
 * read/write return false even after a partial operation. No callback may
 * reenter, resume a vCPU, reset the device or mutate the disk/metadata.
 * Guest is stopped for the entire call. Disk is immutable host-owned memory, never mapped as guest DMA RAM. */
struct vro_io {
 void *ctx;
 bool (*admit)(void *,uint64_t,size_t,bool);
 bool (*read)(void *,uint64_t,void *,size_t);
 bool (*write)(void *,uint64_t,const void *,size_t);
 void (*release)(void *); /* real DMA store barrier required by integration */
};
struct vro {
 struct vro_io io;
 const uint8_t *disk;
 size_t disk_size;
 uint64_t base;
 uint16_t index;
 bool failed, busy;
};
/* Conservative pilot: N=128, legacy allocation 8192 bytes, no indirect/event,
 * only one outstanding head. Init/reset is only legal under exclusive ownership,
 * with no old callback in flight and freshly reset guest queue indices. */
bool vro_init(struct vro *,struct vro_io,const uint8_t *,size_t,uint32_t);
/* 0 idle, 1 completion, -1 latched terminal failure. Never retry after -1.
 * Requires offered RO, transport readiness (legacy first use or DRIVER_OK),
 * BUS_MASTER and polling NO_INTERRUPT;
 * OUT always fails even if the driver did not accept RO.
 * transport admission is caller responsibility and NOT implemented here. */
int vro_poll(struct vro *);
#endif
