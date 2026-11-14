#ifndef SYLE_FIRMWARE_APIC_ACCESSED_H
#define SYLE_FIRMWARE_APIC_ACCESSED_H
#include "firmware_clock_apic_transaction.h"
#include "firmware_dirty_transaction.h"
struct faa_entry {uint64_t gpa,value,token;};
struct faa_plan {
 const struct fmm_memory *live_memory;const struct fat_exit *live_exit;
 const uint64_t *live_gpr;const struct fct_state *live_apic;
 struct fmm_memory memory;struct fat_exit exit;uint8_t device_bytes[sizeof(struct fag_state)];uint32_t ecx,edx;uint64_t new_rax;
 struct sg_memory_ops owner;struct fwl_plan layout;uint64_t eptp,gpr[15];
 struct faa_entry data[3],source[60];unsigned source_count,length;
 uint8_t instruction[15];uint64_t *leaf_pointer,leaf_token,source_tokens[15];
 struct fwd_plan transaction;
};
/* Pure adapter, nondefault cache fixture only: exact8b01/read4/APIC IDfee00020,2MiB supervisor leaf A0/D0,
 * ancestorsA1. Owned RW37 leaf storage and actual absent EPT target; captures
 * all source/device/mode/walk facts. Exact source-entry/data-leaf alias rejects.
 * Not a global guest-alias proof. No old clock snapshot is restored. */
bool faa_capture(const struct fmm_memory*,const struct fat_exit*,const uint64_t gpr[15],
 const struct fct_state*,uint32_t ecx,uint32_t edx,struct faa_plan*);
bool faa_validate(const struct faa_plan*,bool after_accessed);
enum fwd_cas_result faa_compare_set(const struct faa_plan*,void*,
 enum fwd_cas_result(*compare_set)(void*,uint64_t*,uint64_t,uint64_t));
#endif
