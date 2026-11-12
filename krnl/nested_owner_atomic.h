#ifndef SYLE_NESTED_OWNER_ATOMIC_H
#define SYLE_NESTED_OWNER_ATOMIC_H
#include "typed_root_atomic.h"
#include "typed_root_local_provider.h"
#include "nested_domain_dispatch.h"
#include "nested_vcpu.h"
#define NESTED_ATOMIC_SLOTS 8u
#define NESTED_ATOMIC_TRACES 256u
enum nested_atomic_reject_stage {
 NOA_REJECT_STOPPED=1,NOA_REJECT_ACCESS,NOA_REJECT_UNSUPPORTED,
 NOA_REJECT_GLA,NOA_REJECT_PAGEWALK,NOA_REJECT_ALLOWED,
 NOA_REJECT_PRIVATE_TREE,NOA_REJECT_PERMISSIONS,NOA_REJECT_BRIDGE
};
/* First rejected reason-48 operation only. Owner-local, published once with
 * valid last; never admission, cleanup or source-write authority. The raw
 * qualification is retained before profile-specific interpretation. */
struct nested_atomic_rejection {
 uint64_t gpa,qualification,pin_controls;
 uint32_t reason;
 enum nested_atomic_reject_stage stage;
 int bridge_status; /* -1 if the bridge was not called. */
 uint8_t actual_permissions;
 bool permissions_valid;
 uint32_t valid;
};
struct nested_owner_sparse;
struct nested_owner_atomic {
 struct nested_owner_sparse *sparse;
 struct nested_vcpu *vcpu;
 struct nested_domain_dispatch *dispatch;
 struct nested_domain_token token;
 struct typed_root_lease *lease;
 struct tr_atomic_owner atomic;
 struct typed_root_registry bridge;
 struct tr_registry_slot slots[NESTED_ATOMIC_SLOTS];
 struct tr_atomic_binding bindings[NESTED_ATOMIC_SLOTS];
 struct tr_atomic_trace traces[NESTED_ATOMIC_TRACES];
 struct tr_local_provider provider;
 struct tr_local_completed completed[NESTED_ATOMIC_SLOTS];
 struct tr_atomic_config config;
 struct tr_registry_root_provider root_provider;
 void *provider_context;
 unsigned used;
 uint64_t fills,promotions,target_promotions[3],paging_faults,entries,ready_entries;
 uint64_t target_gpa; /* diagnostic counter target; zero retains legacy 0xf000 */
 uint64_t key_lifetime[3],warm_checks;
 bool (*healthy)(void *);
 void *healthy_context;
 /* Optional N42B empty-owner rendezvous, before the first allocation. */
 bool (*empty_ready)(struct nested_owner_atomic *,void *);
 void *empty_context;
 bool physical_ad,failed;
 struct nested_atomic_rejection rejection;
};
bool nested_owner_atomic_stopped(void *);
bool nested_owner_atomic_empty(struct nested_owner_sparse *,const struct nested_sparse_ops *,void *);
bool nested_owner_atomic_initial_select(struct nested_owner_atomic *);
bool nested_owner_atomic_bootstrap(struct nested_owner_sparse *,const struct nested_sparse_ops *,void *);
bool nested_owner_atomic_allowed(void *,uint64_t);
bool nested_owner_atomic_fault(struct nested_owner_atomic *,uint64_t,uint64_t qualification,uint64_t actual_pin_controls);
bool nested_owner_atomic_entry(struct nested_owner_atomic *,uint64_t actual_vmcs,uint64_t actual_eptp,bool l2);
bool nested_owner_atomic_item_leaf(struct nested_owner_atomic *,struct nested_ept_context_item *,uint64_t,uint64_t *);
bool nested_owner_atomic_leaf(struct nested_owner_atomic *,uint64_t gpa,uint64_t *leaf);
#endif
