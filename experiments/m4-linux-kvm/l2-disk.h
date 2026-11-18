#ifndef SYLE_L2_DISK_H
#define SYLE_L2_DISK_H
#include "virtio_blk/rw_backend.h"
#define L2_DISK_BASE UINT64_C(0xd0000000)
#define L2_DISK_IRQ 5
struct l2_disk {
    struct vrw queue;
    struct vrw_store store;
    struct vro_io dma;
    uint32_t device_sel, driver_sel, queue_sel, pfn, size, align, page_size, status;
    bool irq;
};
bool l2_disk_open(struct l2_disk *, const char *path, void *ram);
bool l2_disk_mmio(struct l2_disk *, int vm, unsigned offset, unsigned char *data, unsigned size, bool write);
#endif
