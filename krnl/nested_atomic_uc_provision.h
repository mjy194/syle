#ifndef SYLE_NESTED_ATOMIC_UC_PROVISION_H
#define SYLE_NESTED_ATOMIC_UC_PROVISION_H
#include <stdbool.h>
#include <stdint.h>
#define NAUC_CPUS 2u
#define NAUC_ROOTS 5u
#define NAUC_GLOBAL UINT32_MAX
/* Proofs are assertions of trusted adapters, never inferred from counters. */
#define NAUC_RETAINED (1u<<0)
#define NAUC_STAGED (1u<<1)
#define NAUC_PAT_MTRR_WB (1u<<2)
#define NAUC_ROOT_OWNER (1u<<3)
#define NAUC_GUEST_NEVER_ENTERED (1u<<4)
#define NAUC_CURRENT_UC (1u<<5)
#define NAUC_FINAL_SOURCE_HOST_ALIASES (1u<<6)
#define NAUC_UNPUBLISHED (1u<<7)
#define NAUC_GATE_CLOSED (1u<<8)
struct nauc_plan {
 uint64_t nonce,type_epoch,target_hpa,target_gpa;
 uint64_t host_pte,host_wb_word,host_uc_word,host_lease;
 uint64_t inventory;
 uint64_t eptp[NAUC_ROOTS];
 uint32_t cpu[NAUC_CPUS];
};
enum nauc_phase {
 NAUC_RETAIN=1,NAUC_PREFLIGHT,NAUC_DETACH,NAUC_INVEPT_BEFORE,
 NAUC_HOST_BREAK,NAUC_INVLPG_BEFORE,NAUC_WBINVD_BEFORE,NAUC_HOST_MAKE,
 NAUC_INVLPG_AFTER,NAUC_WBINVD_AFTER,NAUC_AUDIT_UC,NAUC_INSTALL,
 NAUC_INVEPT_AFTER,NAUC_AUDIT_FINAL,NAUC_PUBLISH
};
struct nauc_request {
 struct nauc_plan plan;
 uint64_t ordinal,eptp;
 enum nauc_phase phase;
 uint32_t cpu; /* actual CPU identity, or NAUC_GLOBAL */
 uint32_t required_proofs;
};
struct nauc_receipt {
 struct nauc_request action;
 uint32_t proofs;
 bool completed;
};
typedef bool (*nauc_execute_fn)(void *,const struct nauc_request *,struct nauc_receipt *);
enum nauc_state { NAUC_NEW,NAUC_RUNNING,NAUC_POISONED,NAUC_PUBLISHED };
struct nauc_controller {
 struct nauc_plan plan;
 enum nauc_state state;
 uint64_t completed_actions;
 enum nauc_phase last_completed;
 bool retention_attempted,retained;
};
/* BSP serialized, one shot, stable zero-initialized storage. Never reinitialize
 * after any attempt, release backing, reopen entry gates, or retry uncertain
 * physical work. Caller supplies a unique never-reused bootstrap nonce and
 * trusted staged inventory; this controller is not an authority mint.
 * The hook dispatches actual owner-local operations to both CPUs in VMX root,
 * with guests never entered. Receipt follows completion, not dispatch. It must
 * prove every requested identity against retained real objects, and not merely
 * echo request metadata. No source catalog publication before PUBLISH. No
 * target dereference from HOST_BREAK until HOST_MAKE completes. PUBLISH must
 * atomically fail closed; even an uncertain publication leaves entry gated.
 * All phases, including failures, keep entry gated and backing retained.
 * No callback may mutate controller/plan or reenter this operation. */
bool nauc_run(struct nauc_controller *,const struct nauc_plan *,nauc_execute_fn,void *);
#endif
