#ifndef SYLE_FIRMWARE_DIRTY_TRANSACTION_H
#define SYLE_FIRMWARE_DIRTY_TRANSACTION_H
#include <stdbool.h>
#include <stdint.h>
/* HOST ONLY: callbacks are a reviewed ownership/snapshot boundary, not proof
 * supplied by this pure state machine. No native CAS, VMX or integration. */
struct fwd_plan { uint64_t leaf_entry_gpa,leaf_value,rip,next_rip,rflags; };
enum fwd_cas_result { FWD_CAS_OK,FWD_CAS_CONFLICT,FWD_CAS_UNCERTAIN };
struct fwd_ops {
 bool (*capture)(void*,struct fwd_plan*);
 bool (*validate)(void*,const struct fwd_plan*,bool after_dirty);
 enum fwd_cas_result (*cas)(void*,uint64_t entry_gpa,uint64_t expected,uint64_t desired);
 bool (*sample)(void*);
 bool (*write)(void*,uint32_t field,uint64_t value);
};
/* dirty_changed records known CAS_OK only; false does NOT prove that an
 * UNCERTAIN backend left memory untouched. failed is the absolute resume gate. */
struct fwd_state { bool failed,consumed,dirty_changed,complete; };
/* Fresh zero state, one-shot even on success. !exclusive => zero effects.
 * secondary_c2_verified is external admission for VPID-off/next-entry guest
 * translation flush, not an inference made here. Never clear failed/consumed.
 * Capture/prevalidate/sample precede CAS; exact2MiB leaf only, no A repair.
 * D1 performs noCAS. CAS mismatch never retries. Successful CAS is irreversible;
 * later failure retainsD, marks failed, forbids resume. Uncertain CAS likewise
 * prohibits rollback/retry. RF then RIP; GPR/ROM publication belongs to caller
 * only after success. All callbacks must keep plan immutable and ownership held.
 * sample may update clock/event bookkeeping only, never guest memory/VMCS,
 * page tables or mappings; post-sample failure after CAS stays terminal. */
bool fwd_apply(struct fwd_state*,bool exclusive,bool secondary_c2_verified,void*,const struct fwd_ops*);
#endif
