#ifndef SYLE_NESTED_PARALLEL_H
#define SYLE_NESTED_PARALLEL_H
#include "nested_vcpu.h"
#include <stddef.h>
#ifdef VMX_NESTED_DOMAIN_TEST
#include "nested_domain_dispatch.h"
#endif
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
#include "nested_owner_sparse.h"
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
#include "nested_owner_atomic.h"
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
#include "nested_source_replace_round.h"
#endif
#ifdef VMX_NESTED_ATOMIC_RETRACK_TEST
#include "nested_atomic_retrack.h"
#endif
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
#include "nested_owner_typed.h"
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
#include "nested_owner_ad.h"
#endif
#endif
#if defined(VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST) || defined(VMX_NESTED_ATOMIC_CATALOG_TEST)
#define NESTED_PARALLEL_TRACE_CAPACITY 65536u
#else
#define NESTED_PARALLEL_TRACE_CAPACITY 16384u
#endif
struct nested_parallel_runtime {
    /* Exactly one fixture owner, vcpu.fixture. VMCS HPAs are vcpu.vmcs01/02;
     * views are pre-bound by BSP. No copied fixture/resource ownership. */
    struct nested_vcpu vcpu;
    void *vmcs01_view,*vmcs02_view;
    uint64_t shared_hpa,reserved02; /* preallocated before register02; never aliases vmcs01 */
    unsigned cpu_index;
#ifdef VMX_NESTED_DOMAIN_TEST
    struct nested_domain_dispatch *domain;
    struct nested_domain_token domain_token;
    uint64_t composed_tables[4],composed_eptp;
#ifdef VMX_NESTED_RECLAIM_TEST
    bool (*reclaim_checkpoint)(struct nested_parallel_runtime *);
#ifdef VMX_NESTED_DOMAIN_KICK_TEST
    uint64_t lapic_base;
#endif
#endif
#endif
#ifdef VMX_NESTED_OWNER_SPARSE_TEST
    struct nested_owner_sparse sparse;
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    struct nested_owner_atomic atomic_catalog;
#ifdef VMX_NESTED_ATOMIC_RETRACK_TEST
    struct nested_owner_atomic_maintenance *retrack_ref;
    void (*retrack_fault)(struct nested_parallel_runtime *);
#endif
#ifdef VMX_NESTED_SOURCE_REPLACE_TEST
    struct nested_source_replace_round *source_round;
    bool (*source_bind)(struct nested_parallel_runtime *,bool *,bool *);
#endif
    bool (*atomic_checkpoint)(struct nested_parallel_runtime *,unsigned);
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    struct nested_owner_typed typed;
    bool (*typed_checkpoint)(struct nested_parallel_runtime *,unsigned phase);
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
    bool (*typed_before_entry)(struct nested_parallel_runtime *,bool in_l2);
#endif
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
    struct nested_owner_ad ad;
#ifdef VMX_NESTED_OWNER_AD_RESET_TEST
    bool (*ad_checkpoint)(struct nested_parallel_runtime *,unsigned phase);
#endif
#endif
#if defined(VMX_NESTED_LOCAL_INVEPT_TEST) || defined(VMX_NESTED_OWNER_CONTEXT_TEST)
    unsigned context_stage;
    bool (*local_checkpoint)(struct nested_parallel_runtime *);
    bool (*local_invalidated)(struct nested_parallel_runtime *);
    unsigned local_invalidations;
#endif
#endif
    char trace[NESTED_PARALLEL_TRACE_CAPACITY];
    size_t trace_used;
    bool trace_overflow,retain;
    unsigned result,fault_code;
    uint32_t faulted,done;
};
/* Fresh storage only, BSP before publication to AP. No allocations or VMX.
 * Runtime/trace is single-writer. BSP reads final trace only after observing
 * done acquire; fault observation does not authorize resource reclamation. */
bool nested_parallel_init(struct nested_parallel_runtime *,unsigned cpu_index);
bool nested_parallel_trace_text(struct nested_parallel_runtime *,const char *text);
bool nested_parallel_trace_hex(struct nested_parallel_runtime *,uint64_t value);
/* Failure notification freezes trace and requires retaining owned resources.
 * This does not claim local VMCS clear, VMXOFF or thread termination. */
void nested_parallel_mark_fault(struct nested_parallel_runtime *,unsigned code);
bool nested_parallel_faulted(const struct nested_parallel_runtime *);
/* Caller attests all relevant hardware references are safely gone when safe
 * is true. A prior fault always retains resources even if safe is requested.
 * Completion freezes trace and publishes result/retain with release ordering. */
bool nested_parallel_finish(struct nested_parallel_runtime *,unsigned result,bool safe);
bool nested_parallel_done(const struct nested_parallel_runtime *);
/* AP fail-closed sink: no console/allocator; publishes fault then CLI/HLT.
 * Host-hardware build only, never call from a user-space unit test. */
_Noreturn void nested_parallel_fault_stop(struct nested_parallel_runtime *,unsigned code,const char *reason);
#endif
