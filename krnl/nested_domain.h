#ifndef SYLE_NESTED_DOMAIN_H
#define SYLE_NESTED_DOMAIN_H
#include <stdbool.h>
#include <stdint.h>
#define NESTED_DOMAIN_CPUS 8u
#define NESTED_DOMAIN_OBJECTS 16u
#define NESTED_DOMAIN_NONE 255u
struct nested_domain_token {uint64_t domain_id,incarnation,generation;unsigned slot;};
struct nested_domain_vmcs {
    uint64_t gpa,generation;
    unsigned owner;
    bool used,active,launched,prepared,running;
};
struct nested_domain_cpu {
    uint64_t vmxon,cache_generation,pending_ticket,pending_root;
    unsigned current,pending_type;
    bool root,guest_running;
};
enum nested_domain_gate {NESTED_DOMAIN_OPEN,NESTED_DOMAIN_REVOKING,NESTED_DOMAIN_POISON,NESTED_DOMAIN_MAINTENANCE};
struct nested_domain {
    uint64_t id,incarnation,participants,residency,revoke_epoch,targets,acks;
    uint64_t maintenance_epoch,maintenance_targets,maintenance_acks;
    unsigned maintenance_purpose;
    bool maintenance_published;
    enum nested_domain_gate gate;
    bool published;
    struct nested_domain_cpu cpus[NESTED_DOMAIN_CPUS];
    struct nested_domain_vmcs objects[NESTED_DOMAIN_OBJECTS];
};
struct nested_domain_cpu_token {uint64_t domain_id,incarnation;unsigned cpu;};
struct nested_domain_invept_ticket {
    uint64_t domain_id,incarnation,sequence,root;
    unsigned cpu,type;
};
/* Physical consumer accounting includes L1 AND L2 access to domain backing.
 * enter precedes possible execution; leave requires a verified stop/exit. */
bool nested_domain_guest_enter(struct nested_domain *,struct nested_domain_cpu_token);
bool nested_domain_guest_leave(struct nested_domain *,struct nested_domain_cpu_token);
struct nested_domain_reclaim_ticket {uint64_t domain_id,incarnation,epoch;};
enum nested_domain_outcome {NESTED_DOMAIN_EXIT,NESTED_DOMAIN_EARLY_FAIL,NESTED_DOMAIN_FAILED_ENTRY};
/* Pure serialized L0-owned bookkeeping. Fixed explicit 8-CPU/16-object budget.
 * Fresh init requires externally unique nonzero incarnation; never reinitialize
 * a live/poisoned domain to evade retained-resource obligations. CPU numbers
 * are bounded domain indices, not unvalidated hardware APIC IDs.
 * No allocation, VMX instruction, actual EPT cache or thread synchronization. */
bool nested_domain_init(struct nested_domain *,uint64_t id,uint64_t incarnation,uint64_t cpu_mask);
bool nested_domain_vmxon(struct nested_domain *,unsigned cpu,uint64_t gpa);
bool nested_domain_vmxoff(struct nested_domain *,unsigned cpu);
bool nested_domain_alloc(struct nested_domain *,uint64_t gpa,struct nested_domain_token *out);
bool nested_domain_delete(struct nested_domain *,struct nested_domain_token);
bool nested_domain_load(struct nested_domain *,unsigned cpu,struct nested_domain_token);
bool nested_domain_clear(struct nested_domain *,unsigned cpu,struct nested_domain_token);
bool nested_domain_prepare(struct nested_domain *,unsigned cpu,struct nested_domain_token,bool resume);
/* start marks issued/uncertain execution, not successful entry. Residency is
 * conservative. Separate guest_enter/leave covers physical L1 execution too. */
bool nested_domain_start(struct nested_domain *,unsigned cpu,struct nested_domain_token);
/* Required L2 sequence: prepare -> start -> guest_enter -> verified physical
 * exit -> guest_leave -> finish. L1 alone uses guest_enter/guest_leave without
 * a prepared VMCS object. A prepared object must be started before enter.
 * Outcomes are trusted hardware observations. EXIT requires start; only EXIT
 * confirms the prior hardware entry succeeded; real hardware sets launch
 * state on entry, not on this bookkeeping observation. EARLY_FAIL only cancels prepared BEFORE start. Unknown or
 * uncertain execution outcome requires poison, not a fabricated failure. */
bool nested_domain_finish(struct nested_domain *,unsigned cpu,struct nested_domain_token,enum nested_domain_outcome);
/* Local-only invalidation tickets: type1 identifies aligned root GPA; type2
 * ignores root input. Neither completion nor cache_generation is a global
 * domain flush or a guarantee about another CPU's cached translations. */
bool nested_domain_invept_begin(struct nested_domain *,unsigned cpu,unsigned type,uint64_t root,struct nested_domain_invept_ticket *ticket);
bool nested_domain_invept_complete(struct nested_domain *,struct nested_domain_invept_ticket ticket);
/* L0 reclamation is a different protocol: targets snapshot ALL historical
 * residency, even CPUs now VMXOFF. Caller must obtain actual safe invalidation
 * or equivalent completion before ACK; metadata itself proves no flush.
 * Preparation, running entries and local pending invalidations block begin. */
bool nested_domain_reclaim_begin(struct nested_domain *,struct nested_domain_reclaim_ticket *ticket);
bool nested_domain_reclaim_publish(struct nested_domain *,struct nested_domain_reclaim_ticket ticket);
bool nested_domain_reclaim_ack(struct nested_domain *,unsigned cpu,struct nested_domain_reclaim_ticket ticket);
bool nested_domain_reclaim_complete(struct nested_domain *,struct nested_domain_reclaim_ticket ticket);
enum nested_domain_maintenance_purpose {NESTED_DOMAIN_AD_RESET=1,NESTED_DOMAIN_AD_OBSERVE=2,NESTED_DOMAIN_TYPED_TRANSITION=3,NESTED_DOMAIN_TYPED_OBSERVE=4,NESTED_DOMAIN_SOURCE_REPLACE=5};
struct nested_domain_maintenance_ticket {uint64_t domain_id,incarnation,epoch;unsigned purpose;};
/* Maintenance is NOT backing reclamation. All participant virtual roots must
 * be active and all physical L1/L2 consumers stopped before begin. History is
 * snapshotted, never caller-supplied. RESET ACK attests local reset/INVEPT;
 * OBSERVE ACK attests stopped observation readiness, not necessarily a flush.
 * After ALL ACKs, publish attests safe clear/stamp preparation or completed
 * observation, then complete reopens the consumer gate. No instructions or table writes
 * here. Even observation has a separate ticket/epoch and explicit completion.
 * Actual physical VMX-root status and updater drain are caller obligations. */
bool nested_domain_maintenance_begin(struct nested_domain *,unsigned purpose,struct nested_domain_maintenance_ticket *);
bool nested_domain_maintenance_publish(struct nested_domain *,struct nested_domain_maintenance_ticket);
bool nested_domain_maintenance_ack(struct nested_domain *,unsigned cpu,struct nested_domain_maintenance_ticket);
bool nested_domain_maintenance_complete(struct nested_domain *,struct nested_domain_maintenance_ticket);
void nested_domain_fail(struct nested_domain *);
#endif
