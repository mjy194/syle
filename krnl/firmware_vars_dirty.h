#ifndef SYLE_FIRMWARE_VARS_DIRTY_H
#define SYLE_FIRMWARE_VARS_DIRTY_H
#include "firmware_vars_rom.h"
#include "firmware_dirty_transaction.h"
struct fvd_entry {uint64_t gpa,value,token;};
struct fvd_plan {
 const struct fmm_memory *live_memory;const struct fat_exit *live_exit;
 const uint64_t *live_gpr;const struct fvr_state *live_probe;
 struct fmm_memory memory;struct fat_exit exit;struct fvr_state probe;
 struct sg_memory_ops owner;struct fwl_plan layout;uint64_t eptp,gpr[15];
 struct fvd_entry data[3],source[60];unsigned source_count,length;
 uint8_t instruction[15];uint64_t *leaf_pointer,leaf_token,source_tokens[15];
 struct fwd_plan transaction;
};
/* Pure validation adapter, no native CAS/VMX inside this module. Stable exclusive stopped-vCPU ownership and
 * immutable callback mappings required across capture/validate/commit. Planner
 * admits exact q58A byte MOV ignored-ROM probe,2MiB supervisor RW/allA leaf.
 * Only missingD is tolerated in this separate plan; old fmm validators unchanged.
 * Source bytes and every consumed-byte walk captured. Unique leaf-storage RAM
 * HPA, actual EPT RW37, no active-ancestor/source-page alias.
 * Conservative source-role gate rejects any source walk entry at the exact
 * data leaf entry GPA; same table page with different entries remains valid.
 * This is not a proof against every possible secondary guest VA/page-table role. Internal host
 * pointers/tokens must never be guest-visible. Trusted plan/output disjoint. */
bool fvd_capture(const struct fmm_memory*,const struct fat_exit*,const uint64_t gpr[15],
 const struct fvr_state*,struct fvd_plan*);
bool fvd_validate(const struct fvd_plan*,bool after_dirty);
/* Revalidates before invoking trusted backend CAS. No implementation of host
 * atomic instructions supplied. Exact old->old|D only; no retry. Backend must
 * provide aligned WB coherent atomicity and uncertain-effect failure reporting.
 * Transaction owner must permanently latch on any non-OK or later failure. */
enum fwd_cas_result fvd_compare_set(const struct fvd_plan*,void*,
 enum fwd_cas_result(*compare_set)(void*,uint64_t*,uint64_t,uint64_t));
#endif
