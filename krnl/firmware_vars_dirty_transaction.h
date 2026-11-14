#ifndef SYLE_FIRMWARE_VARS_DIRTY_TRANSACTION_H
#define SYLE_FIRMWARE_VARS_DIRTY_TRANSACTION_H
#include "firmware_vars_dirty.h"
/* Caller-owned STATIC/heap context, zero initially; persistent one-shot, never
 * reset after fwd is entered. Only the nondefault cache fixture wires native callbacks. Plan holds host-only pointers.
 * Each successful probe command needs a distinct fresh context, not reuse. */
struct fvdt_state {
 struct fwd_state transaction;struct fvd_plan plan;struct fvr_state staged;
 void *callback_context;
 bool (*sample)(void*);
 enum fwd_cas_result (*cas)(void*,uint64_t*,uint64_t,uint64_t);
 bool (*write)(void*,uint32_t,uint64_t);
};
/* D0-only route. Admission/capture/noD rejects before effects => FAT_REJECTED.
 * Once fwd begins, ANY failure permanently marks vars.failed => FAT_FATAL.
 * ROM stage publishes only after postvalidation/sample, RF and RIP succeed.
 * !exclusive zero effects. c2_verified is external VPID-off/flush admission.
 * Callbacks must not modify this context, plan, source, mappings or ROM state;
 * sample modifies clock/event state only. CAS uncertainty can leave D set even
 * when transaction.dirty_changed=false: never rollback/retry on failure. */
enum fat_result fvdt_apply(struct fvdt_state*,struct fvr_state*,uint64_t gpr[15],
 const struct fmm_memory*,const struct fat_exit*,bool exclusive,bool c2_verified,void*,
 bool(*sample)(void*),enum fwd_cas_result(*cas)(void*,uint64_t*,uint64_t,uint64_t),
 bool(*write)(void*,uint32_t,uint64_t));
#endif
