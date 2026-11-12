#ifndef SYLE_DOMAIN_TYPED_ALIAS_PLAN_H
#define SYLE_DOMAIN_TYPED_ALIAS_PLAN_H
#include "domain_typed_inventory.h"
struct domain_typed_alias_slot {uint64_t *pointer;uint64_t table_hpa,raw;unsigned index;};
enum domain_typed_alias_phase { DTA_EMPTY,DTA_PREPARED,DTA_DETACHED,DTA_PUBLISHED,DTA_FAILED };
struct domain_typed_alias_plan {
    struct domain_typed_request request;
    struct domain_typed_root_binding roots[DOMAIN_TYPED_ROOTS];
    struct domain_typed_inventory before,detached;
    struct domain_typed_alias_slot slots[DOMAIN_TYPED_ALIASES];
    domain_typed_table_resolver resolve;
    void *context;
    uint64_t epoch;
    unsigned source_type,destination_type,nonwb_type;
    enum domain_typed_alias_phase phase;
};
struct domain_typed_alias_workspace {
    struct domain_typed_workspace scan;
    struct domain_typed_inventory inventory;
    uint64_t overlay_hpa[DOMAIN_TYPED_ALIASES];
    unsigned overlay_count;
    _Alignas(4096) uint64_t overlay[DOMAIN_TYPED_ALIASES][512];
};
/* Caller-owned workspace is large: never allocate on a kernel stack.
 * Fresh plan only, frozen/pinned owned WB table RAM, exclusive writer.
 * The inventory resolver supplies a const view; this API additionally requires
 * caller authority to modify that exact owned RAM view, not guest permissions.
 * WB(6)<->UC(0), IPAT1 only; expected_source is this epoch's actual source.
 * No target memory reads, INVEPT/cache operations, publication synchronization
 * or pin acquisition. Caller completes those steps in the surrounding protocol.
 * Any failed operation sets FAILED: retain and stop, never blindly retry.
 * All rejection checks precede table writes; ordinary pinned RAM stores cannot
 * fail under this contract. Full-scan digests detect mistakes, not adversaries.
 * Detach/publish preserve bindings and non-target mappings, including spare
 * roots with historical_mask zero and historical roots with zero aliases. */
bool domain_typed_alias_prepare(struct domain_typed_alias_plan *,
    const struct domain_typed_request *,unsigned expected_source,unsigned destination,
    uint64_t epoch,domain_typed_table_resolver,void *,struct domain_typed_alias_workspace *);
bool domain_typed_alias_detach(struct domain_typed_alias_plan *,uint64_t epoch,
    struct domain_typed_alias_workspace *);
bool domain_typed_alias_publish(struct domain_typed_alias_plan *,uint64_t epoch,
    struct domain_typed_alias_workspace *);
/* Explicit target policy 0=UC or 4=WT. Only WB<->selected type;
 * UC<->WT is never a supported transition. Old prepare remains UC-only. */
bool domain_typed_alias_prepare_type(struct domain_typed_alias_plan *,
    const struct domain_typed_request *,unsigned expected_source,unsigned destination,
    uint64_t epoch,unsigned nonwb_type,domain_typed_table_resolver,void *,
    struct domain_typed_alias_workspace *);
#endif
