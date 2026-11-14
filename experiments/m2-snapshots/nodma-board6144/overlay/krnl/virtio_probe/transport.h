#ifndef SYLE_HOST_VIRTIO_TRANSPORT_H
#define SYLE_HOST_VIRTIO_TRANSPORT_H
#include "backend.h"
/* Isolated polling-only legacy PCI function model. Does NOT allocate/rout a
 * platform BAR/IRQ, implement CF8/CFC decode or bind any real guest driver. */
struct vrt {
 struct vro backend;
 struct vro_io io;
 const uint8_t *disk;
 size_t disk_size;
 uint32_t features,pfn,bar;
 uint16_t command,select;
 uint8_t status;
 bool probe,failed,started;
};
bool vrt_init(struct vrt *,struct vro_io,const uint8_t *,size_t);
/* Byte/word/dword aligned configuration transactions within 256 bytes.
 * false latches terminal failure. Reads and writes are little endian values. */
bool vrt_config(struct vrt *,unsigned offset,unsigned width,bool write,uint32_t *);
/* Absolute IO port; exact register widths only. Caller provides exclusive
 * device/guest ownership through completion of enclosing IO instruction.
 * An instruction-commit failure must terminally stop even after success here. */
bool vrt_io(struct vrt *,uint16_t port,unsigned width,bool write,uint32_t *);
#endif
