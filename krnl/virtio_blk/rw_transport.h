#ifndef SYLE_VIRTIO_RW_TRANSPORT_H
#define SYLE_VIRTIO_RW_TRANSPORT_H
#include "rw_backend.h"
/* Legacy PCI function with explicit INTx IRQ5 and flushable backing. Does NOT allocate/route a
 * platform BAR/IRQ, implement CF8/CFC decode or bind any real guest driver. */
struct vrwt {
 struct vrw backend;
 struct vro_io io;
 struct vrw_store disk;
 uint64_t reads,writes,flushes,completions;
 uint32_t features,pfn,bar;
 uint16_t command,select;
 uint8_t status;
 bool probe,failed,started;
};
bool vrwt_init(struct vrwt *,struct vro_io,struct vrw_store);
/* Byte/word/dword aligned configuration transactions within 256 bytes.
 * false latches terminal failure. Reads and writes are little endian values. */
bool vrwt_config(struct vrwt *,unsigned offset,unsigned width,bool write,uint32_t *);
/* Absolute IO port; exact register widths only. Caller provides exclusive
 * device/guest ownership through completion of enclosing IO instruction.
 * An instruction-commit failure must terminally stop even after success here. */
bool vrwt_io(struct vrwt *,uint16_t port,unsigned width,bool write,uint32_t *);
bool vrwt_irq(const struct vrwt*);
#endif
