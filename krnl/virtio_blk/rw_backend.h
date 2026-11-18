#ifndef SYLE_VIRTIO_RW_BACKEND_H
#define SYLE_VIRTIO_RW_BACKEND_H
#include "backend.h"
/* Exclusive synchronous storage. Success of flush means stable backing;
 * failure may follow partial I/O and poisons the entire device. No callback
 * may reenter, mutate queue metadata, or resume the guest. */
struct vrw_store {
 void *ctx;uint64_t sectors;
 bool (*read)(void*,uint64_t,uint8_t[512]);
 bool (*write)(void*,uint64_t,const uint8_t[512]);
 bool (*flush)(void*);
};
struct vrw {
 struct vro_io io;struct vrw_store store;uint64_t base;
 uint64_t reads,writes,flushes,completions;uint16_t index;
 bool failed,busy,interrupt;
};
bool vrw_init(struct vrw*,struct vro_io,struct vrw_store,uint32_t);
/* One completion per call; transport drains at most128 heads per notify.
 * IN/OUT/FLUSH/GET_ID, no indirect or EVENT_IDX. All descriptor admission
 * precedes mutation. Successful OUT is flushed before status/used publish. */
int vrw_poll(struct vrw*);
#endif
