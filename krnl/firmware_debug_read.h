#ifndef SYLE_FIRMWARE_DEBUG_READ_H
#define SYLE_FIRMWARE_DEBUG_READ_H
#include "firmware_apic_transaction.h"
#include "linux_external_irq.h"
/* HOST-ONLY fixed virtual clean debug bank, NOT hardware reset contents or
 * host DR state. DR0-3=0, DR6=ffff0ff0, DR7=400. No writable bank, no debug event
 * support: writes/DR4/5/other GPR fail closed. Caller stops on any debug event
 * before further entry. debug_events_absent is whole-session exception flow
 * provenance, not inferred from DR7. Caller checks #DB interception and fcmd
 * debug admission on every read. RTM/advanced debug features remain hidden.
 * DR6 upper32=0, B0-3/BD/BS/BT clear, bit16=1; no write-clear support.
 * No host debug register read/write occurs here.
 * Fresh private state once per domain/VMCS lifetime; no reset/copy/rollback.
 * Caller owns actual VMCS identity/control/debug-event provenance and stable
 * owned instruction/mode snapshot. Bank cannot authenticate these inputs.
 * Failure after VMwrite permanently poisons bank; native caller must propagate
 * that failure to its entire session and forbid entry/retry. */
struct fdr_bank {uint64_t domain,vmcs;bool ready,busy,failed;};
struct fdr_gate {uint64_t domain,vmcs,primary_controls,debugctl;bool debug_events_absent;};
bool fdr_init(struct fdr_bank*,uint64_t domain,uint64_t vmcs);
enum fat_result fdr_read(struct fdr_bank*,const struct fdr_gate*,uint64_t[15],
 const struct fmm_memory*,const struct fat_exit*,unsigned length,bool exclusive,
 void*,lei_write);
#endif
