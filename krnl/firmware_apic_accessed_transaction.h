#ifndef SYLE_FIRMWARE_APIC_ACCESSED_TRANSACTION_H
#define SYLE_FIRMWARE_APIC_ACCESSED_TRANSACTION_H
#include "firmware_apic_accessed.h"
struct faat_state {struct faa_plan plan;bool consumed,failed,accessed_known,complete;};
/* Static/heap one-shot context. Only the nondefault cache fixture wires native callbacks. Full validation,
 * sample(live clock only), snapshot revalidation/CAS, strict postvalidation,
 * sample, RF, RIP, then EAX publish. No device/clock snapshot copy-back.
 * Every transaction failure latches this state + active.failed + timer.terminal.
 * UNCERTAIN can set A even with accessed_known=false. Never rollback/retry.
 * !exclusive has no effects; C2 is external VPID-off/next-entry flush admission.
 * Callbacks must keep snapshot/device/guest state immutable except exact CAS
 * and specified VMCS writes; samples may update only live clock/event facts. */
enum fat_result faat_apply(struct faat_state*,struct fct_state*,uint64_t gpr[15],
 const struct fmm_memory*,const struct fat_exit*,uint32_t ecx,uint32_t edx,
 bool exclusive,bool c2_verified,void*,bool(*sample)(void*),
 enum fwd_cas_result(*cas)(void*,uint64_t*,uint64_t,uint64_t),bool(*write)(void*,uint32_t,uint64_t));
#endif
