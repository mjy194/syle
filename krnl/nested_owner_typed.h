#ifndef SYLE_NESTED_OWNER_TYPED_H
#define SYLE_NESTED_OWNER_TYPED_H
#include "nested_owner_sparse.h"
#include "domain_typed_lease.h"
struct nested_owner_typed {
    struct nested_owner_sparse *owner;
    struct nested_ept_sparse *tree;
    uint64_t eptp12;
    struct domain_typed_lease *lease;
    struct dtl_token *token;
    uint64_t nonce,epoch,target_hpa,misses;
    unsigned root_index;
    bool (*environment_guard)(void *);
    void *guard_context;
    /* Required thread-safe fatal notification (atomic latch), NOT DTL writes. */
    void (*fatal)(void *);
    void *fatal_context;
    bool bound,failed,allow_wt;
};
/* BSP configures stable lease/token, saved nonce/epoch, target and mandatory
 * externally verified CD/NW/cache-maintenance guard. Caller serializes lease
 * metadata and freezes mappings while owner runs. Bind is local owner after
 * fresh AD0 sparse tree initialization; bind only installs callback, lease may
 * be initialized later. Caller forbids entry until inventory/token publication.
 * No allocation/PMM/console here. Failure only marks this owner and calls
 * fatal; shared DTL poison requires coordinated BSP after consumers stop.
 * Local failure never authorizes cleanup, transition, or backing release. */
bool nested_owner_typed_bind(struct nested_owner_typed *,struct nested_owner_sparse *);
/* Fresh registry item, separate stable typed state per item. The lease/token
 * may be published later; all admission fails until it is valid. No eviction
 * or state rebinding is supported by this bounded two-context adapter. */
bool nested_owner_typed_bind_context(struct nested_owner_typed *,struct nested_owner_sparse *,struct nested_ept_context_item *);
bool nested_owner_typed_miss(struct nested_owner_typed *,uint64_t gpa,uint8_t access);
/* Owner after successful empty-tree reset; token already prepared by the
 * global transition. STAMP or OPEN phase, same saved nonce, next epoch. Does not
 * open lease or modify token; OPEN must follow all owners' stamp updates. */
bool nested_owner_typed_refresh(struct nested_owner_typed *,uint64_t epoch,uint64_t nonce);
#endif
