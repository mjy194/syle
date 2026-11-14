#ifndef SYLE_VIRTIO_WB_PUBLICATION_PRIMITIVE_H
#define SYLE_VIRTIO_WB_PUBLICATION_PRIMITIVE_H
/* x86-64 compiler+MFENCE primitive for already admitted stable WB CPU mappings.
 * NOT a DMA callback, ownership proof, mapping pin, vCPU stop or cache flush.
 * Future transaction: data/status/used stores -> this -> used.idx store.
 * Does not authorize any store, resume, external-DMA access or MMIO use. */
void vro_wb_publish(void);
#endif
