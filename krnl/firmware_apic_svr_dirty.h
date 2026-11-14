#ifndef SYLE_FIRMWARE_APIC_SVR_DIRTY_H
#define SYLE_FIRMWARE_APIC_SVR_DIRTY_H
#include "firmware_clock_apic_transaction.h"
#include "firmware_dirty_transaction.h"
struct fsd_entry {uint64_t gpa,value,token;};
struct fsd_plan {
 const struct fmm_memory *live_memory;const struct fat_exit *live_exit;
 const uint64_t *live_gpr;const struct fct_state *live_apic;
 struct fmm_memory memory;struct fat_exit exit;uint8_t device_bytes[sizeof(struct fag_state)];uint32_t ecx,edx;struct fag_state staged_device;
 struct sg_memory_ops owner;struct fwl_plan layout;uint64_t eptp,gpr[15];
 struct fsd_entry data[3],source[60];unsigned source_count,length;
 uint8_t instruction[15];uint64_t *leaf_pointer,leaf_token,source_tokens[15];
 struct fwd_plan transaction;
};
/* HOST ONLY: exact41891c24 MOV[R12],EBX, value10f, write4fee000f0,
 * exact2MiB supervisor leafFEE000A3->E3; ancestorsA1. Real fag SVR staged
 * effect, no ROM model. Actual EPT hole and ownedRW37 table storage, frozen
 * source/CPU/device/owner snapshot; exact source-entry/data-leaf alias rejects.
 * Live clock is not snapshotted/restored. DisabledSVR+armed timer rejects.
 * Internal pointers never guest-visible; no global guest-alias proof. */
bool fsd_capture(const struct fmm_memory*,const struct fat_exit*,const uint64_t gpr[15],
 const struct fct_state*,uint32_t ecx,uint32_t edx,struct fsd_plan*);
bool fsd_validate(const struct fsd_plan*,bool after_dirty);
enum fwd_cas_result fsd_compare_set(const struct fsd_plan*,void*,
 enum fwd_cas_result(*compare_set)(void*,uint64_t*,uint64_t,uint64_t));
#endif
