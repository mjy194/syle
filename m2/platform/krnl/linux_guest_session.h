#ifndef SYLE_LINUX_GUEST_SESSION_H
#define SYLE_LINUX_GUEST_SESSION_H
#include "guest_control.h"
#include "linux_vcpu_backend.h"
#include "linux_vcpu_native.h"
/* Persistent singleton, BSP only. VMCS remains current across STOP/START;
 * stop means quiesced, NOT VMCLEAR. Destroy performs full teardown. */
struct lgs_session;
/* Outcome from THIS lvs_run only. Invalid fields never imply nondelivery.
 * In particular GUEST_CAPTURE_FAILED is unknown, not entry_status reuse. */
struct lgs_entry_result {
 enum lvs_result state_result;
 bool fields_valid;
 uint32_t reason,vectoring;
};
struct lgs_platform {
 void *ctx;
 /* enable must either succeed or leave host unchanged; saves host CR4 and
  * supplies a safe fallback host RIP/RSP. disable runs only after VMXOFF. */
 bool (*enable)(void *);
 bool (*disable)(void *);
 bool (*safe)(void *);
 bool (*read_field)(void *,uint32_t,uint64_t *);
 /* Handle one real VM exit. false forbids any further entry until destroy.
  * Must preserve host state and not free/change session ownership. */
 bool (*dispatch)(void *,struct lgs_session *,uint32_t);
 void (*fatal)(void *);
 /* Optional pair: before runs after timer setup; false forbids entry.
  * after runs exactly once after a successful before and host-safe lvs_run,
  * before reason/vectoring rejection. Dirty host is fatal, never a callback.
  * false from either latches failure: caller must stop/destroy, not retry. */
 bool (*before_entry)(void *,struct lgs_session *);
 bool (*after_entry)(void *,struct lgs_session *,const struct lgs_entry_result *);
 /* Optional after owned RAM prepare, before enable; false retains ownership. */
 bool (*prepare_guest)(void *,struct lgs_session *);
};
struct lgs_session {
 struct linux_guest_memory memory;
 struct lvh_context host;
 struct lvn_context native;
 struct lvs_state state;
 struct lvh_ops hop;
 struct lvs_ops sop;
 struct lgs_platform platform;
 const void *kernel,*initrd;
 size_t kernel_size,initrd_size;
 const char *command_line;
 uint64_t fallback_rsp,fallback_rip;
 bool configured,enabled,prepared,failed;
 uint64_t exits,last_reason;
 /* Last attempted CREATE substage, retained for bounded diagnostics. */
 unsigned prepare_stage;
 unsigned step_stage,step_result;
};
/* Before publication, initialize a fresh zeroed object with immutable input
 * buffers, validated ops and aligned resident fallback stack/RIP. No hardware. */
bool lgs_configure(struct lgs_session *,const struct lvh_ops *,const struct lvs_ops *,
 const struct lgs_platform *,const void *,size_t,const void *,size_t,const char *,uint64_t,uint64_t);
extern const struct guest_backend lgs_guest_backend;
#endif
