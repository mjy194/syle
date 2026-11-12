#ifndef SYLE_NESTED_OWNER_SPARSE_H
#define SYLE_NESTED_OWNER_SPARSE_H
#include "nested_owner_pool.h"
#include "ept.h"
#include "nested_ept_contexts.h"
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
#include "typed_root_overlap.h"
#endif
#ifdef VMX_NESTED_OWNER_CONTEXT_TEST
#define NESTED_OWNER_POOL_PAGES 96u
#else
#define NESTED_OWNER_POOL_PAGES 64u
#endif
struct nested_owner_atomic;
struct nested_owner_ad;
struct nested_owner_typed;
struct nested_owner_dynamic_typed;
struct nested_owner_sparse {
#ifdef VMX_NESTED_ATOMIC_CATALOG_TEST
    struct nested_owner_atomic *atomic_catalog;
#endif
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    struct tr_overlap_switch overlap_switch;
    uint64_t selecting12;
#endif
#ifdef VMX_NESTED_OWNER_TYPED_DYNAMIC_TEST
    struct nested_owner_dynamic_typed *dynamic_typed;
#endif
#ifdef VMX_NESTED_OWNER_TYPED_TEST
    struct nested_owner_typed *typed;
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
    struct nested_owner_typed *typed_contexts[2];
    const struct nested_context_transactions *typed_transactions;
    void *typed_transaction_context;
    bool (*contexts_ready)(struct nested_owner_sparse *);
    bool (*refill_begin)(struct nested_owner_sparse *);
    bool (*refill_end)(struct nested_owner_sparse *);
#endif
#endif
#ifdef VMX_NESTED_OWNER_AD_TEST
    struct nested_owner_ad *atomic_ad;
#endif
    struct nested_owner_pool pool;
    struct nested_sparse_page pages[NESTED_OWNER_POOL_PAGES];
    struct nested_owner_slot slots[NESTED_OWNER_POOL_PAGES];
    struct nested_ept_sparse tree;
    struct nested_ept_contexts contexts;
    struct nested_ept_context_item *hardware_item;
    bool multiple,evict;
    uint64_t creations,evictions;
    size_t peak_live;
    uint64_t selected12;
    struct ept_fixture *fixture;
    uint64_t misses;
    bool initialized,destroyed;
};
/* BSP owns/preallocates pages. All other methods execute only on bound owner
 * in VMX root; no PMM/console. Failure requires caller retain+stop. */
#ifdef VMX_NESTED_OWNER_TYPED_CONTEXT_TEST
/* Runtime configures both stable states and transactions before start. */
struct nested_owner_typed *nested_owner_sparse_typed_context(struct nested_owner_sparse *,uint64_t key);
/* Exact production bootstrap, separated from privileged local INVEPT backend
 * so assembly and failure paths can be tested with trusted injected ops.
 * start has already set initialized/fixture/selected12; fresh contexts only. */
bool nested_owner_sparse_bootstrap_contexts(struct nested_owner_sparse *,const struct nested_sparse_ops *,void *);
#endif
bool nested_owner_sparse_start(struct nested_owner_sparse *,struct ept_fixture *);
bool nested_owner_sparse_miss(struct nested_owner_sparse *,uint64_t gpa,uint8_t access);
bool nested_owner_sparse_select(struct nested_owner_sparse *,uint64_t);
bool nested_owner_sparse_invalidate(struct nested_owner_sparse *,uint64_t,uint64_t);
bool nested_owner_sparse_reset(struct nested_owner_sparse *);
bool nested_owner_sparse_stop(struct nested_owner_sparse *);
#endif
