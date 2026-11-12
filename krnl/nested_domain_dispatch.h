#ifndef SYLE_NESTED_DOMAIN_DISPATCH_H
#define SYLE_NESTED_DOMAIN_DISPATCH_H
#include "nested_domain.h"
#include "nested_vmx.h"
#define NESTED_DISPATCH_LOCK_TRIES 1048576u
enum nested_internal_origin { NESTED_INTERNAL_NONE,NESTED_INTERNAL_KICK,NESTED_INTERNAL_EPT };
struct nested_domain_dispatch {
    struct nested_domain domain;
    struct nested_domain_token tokens[2];
    uint32_t lock,fault_latch;
    bool internal_pending[2],internal_resumed[2],internal_used[2];
    enum nested_internal_origin internal_origin[2];
    uint32_t ept_suspends[2];
};
/* BSP fresh init; fixed CPU0/1, virtual VMXON d000/2d000 and VMCS12 e000/2e000.
 * Each nvmx model is private to its physical owner. This lock is metadata only:
 * never held over VM entry, allocation, console, invalidation or IPI waits.
 * Caller establishes IF=0/operand validity before entering dispatch. */
bool nested_domain_dispatch_init(struct nested_domain_dispatch *,uint64_t id,uint64_t incarnation);
/* false is profile/identity/model inconsistency, NOT architectural VMfail.
 * On rejection, domain/local/out unchanged. Lock timeout sets fatal latch.
 * True returns the actual nvmx model result, including architectural failures. */
bool nested_domain_dispatch_execute(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local,struct nvmx_request,struct nvmx_result *out);
bool nested_domain_dispatch_physical_enter(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local,bool in_l2);
/* status==0 and reason bit31==0 required. Failure poisons and retains.
 * For L2 this commits domain launch + nvmx_entry_commit exactly once; adapter
 * must NOT repeat entry_commit. Adapter then performs exit_snapshot and
 * nvmx_return_from_l2 on private local state before next virtual instruction.
 * Internal pauses use the explicit pause/suspend/resume APIs below. */
bool nested_domain_dispatch_physical_leave(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local,bool in_l2,unsigned status,uint64_t reason);
/* One internal L2 interrupt pause per CPU in this bounded diagnostic. Adapter
 * verifies exit interruption info/ISR/IF/RIP before pause(status=0,reason=1).
 * Pause confirms entry once but leaves nvmx.running true for later reflection.
 * Resume requires reopened domain gate and only registers physical consumption;
 * final leave(reason=18) must not commit launch a second time. */
bool nested_domain_dispatch_physical_pause(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local,unsigned status,uint64_t reason);
/* Repeatable internal EPT miss, exact physical reason48/status0 only.
 * Adapter validates/repairs the actual fault separately. First suspend commits
 * entry once; subsequent suspends only release physical consumption. EPT and
 * kick origins cannot mix in this diagnostic. Counter overflow is rejected
 * without mutation; caller must stop rather than attempt untracked reentry. */
bool nested_domain_dispatch_physical_suspend_ept(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local,unsigned status,uint64_t reason);
bool nested_domain_dispatch_physical_resume(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local);
/* Reclaim metadata only. begin closes entry and snapshots historical residency.
 * Caller publishes mappings and performs per-CPU invalidation OUTSIDE this lock;
 * publish/ack attest those actions, not execute them. All operations validate the
 * full domain/incarnation/epoch ticket. Policy rejection is failure-atomic;
 * hardware failure/timeout must call markfail and retain backing. */
bool nested_domain_dispatch_reclaim_begin(struct nested_domain_dispatch *,
    struct nested_domain_reclaim_ticket *out);
bool nested_domain_dispatch_reclaim_publish(struct nested_domain_dispatch *,
    struct nested_domain_reclaim_ticket);
bool nested_domain_dispatch_reclaim_ack(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_reclaim_ticket);
bool nested_domain_dispatch_reclaim_complete(struct nested_domain_dispatch *,
    struct nested_domain_reclaim_ticket);
/* Synchronized physical-consumer check. Virtual running/prepared may remain
 * true during internal EPT suspension and are deliberately not rejected. */
bool nested_domain_dispatch_consumer_stopped(struct nested_domain_dispatch *,unsigned cpu,struct nested_domain_token);
/* Exact closed-round checkpoint query; synchronized and remote-safe. Requires
 * a complete token, AD_RESET ticket and exact two-owner historical target set.
 * Virtual running is not physical consumption. No internal suspend may remain. */
bool nested_domain_dispatch_maintenance_stopped(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nested_domain_maintenance_ticket);
/* Read-only open-gate source checkpoint and expected next ticket. Does not
 * reserve an epoch or close entry. Caller binds this expected ticket before
 * close, then verifies the actual begin ticket exactly; any intervening round
 * makes this plan stale. Rejection leaves output unchanged (lock timeout still
 * latches failure). Same owner-adapter full-leave obligation as source_stopped. */
bool nested_domain_dispatch_source_plan(struct nested_domain_dispatch *,
 struct nested_domain_maintenance_ticket *out_expected_ticket);
/* Exact SOURCE_REPLACE checkpoint, not AD_RESET or arbitrary maintenance.
 * Both fixed owners must remain active/current/launched, fully physically left,
 * with complete history3 and no internal suspension. Checks both owners under
 * lock. Proves dispatch-owned state only. Before invoking a source close hook,
 * each owner adapter must finish private nvmx exit reflection and
 * nvmx_return_from_l2 after full physical leave. Internal suspend is insufficient. */
bool nested_domain_dispatch_source_stopped(struct nested_domain_dispatch *,unsigned,
 struct nested_domain_token,struct nested_domain_maintenance_ticket);
void nested_domain_dispatch_markfail(struct nested_domain_dispatch *);
/* Final success requires both CPUs historically resident (mask3), no active
 * objects/current/root/prepared/running/consumer or pending invalidation. */
bool nested_domain_dispatch_final(struct nested_domain_dispatch *);
/* Owner-local INVEPT scope only, not a global reclaim acknowledgement.
 * No hardware flush under lock. Complete attests successful actual local flush;
 * on hardware failure caller must markfail, retain, and never complete. */
bool nested_domain_dispatch_local_invept_begin(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local,unsigned type,uint64_t root,
    struct nested_domain_invept_ticket *out);
bool nested_domain_dispatch_local_invept_complete(struct nested_domain_dispatch *,unsigned cpu,
    struct nested_domain_token,struct nvmx *local,struct nested_domain_invept_ticket);
/* Short-lock maintenance bookkeeping only; never flush/wait while locked.
 * SOURCE_REPLACE alone requires the exact two-owner full-leave checkpoint
 * before issuing a ticket. Other maintenance purposes retain their contract. */
bool nested_domain_dispatch_maintenance_begin(struct nested_domain_dispatch *,unsigned purpose,struct nested_domain_maintenance_ticket *);
bool nested_domain_dispatch_maintenance_publish(struct nested_domain_dispatch *,struct nested_domain_maintenance_ticket);
bool nested_domain_dispatch_maintenance_ack(struct nested_domain_dispatch *,unsigned cpu,struct nested_domain_maintenance_ticket);
bool nested_domain_dispatch_maintenance_complete(struct nested_domain_dispatch *,struct nested_domain_maintenance_ticket);
#endif
