#ifndef SYLE_NESTED_OWNER_DYNAMIC_TYPED_H
#define SYLE_NESTED_OWNER_DYNAMIC_TYPED_H
#include "nested_owner_sparse.h"
#include "typed_root_registry.h"
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
#include "nested_root_catalog.h"
#endif
/* Stable owner state; each actual registry item has its own stable typed
 * binding. No fixed root index or prebuilt B/C root is part of this adapter. */
struct nested_owner_dynamic_typed {
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
 struct nested_root_catalog *catalog,*bound_catalog;
 struct nrc_identity catalog_identity;
 struct nrc_root selected_source;
 struct nrc_session source_session; /* retained if release fails */
 uint64_t source_nonce,initial_key;
 struct tr_registry_walk walk;
 struct nested_ept_result last_translation;
 uint64_t catalog_checks;
#endif
 struct nested_owner_sparse *owner;
 struct typed_root_registry *bridge;
 bool (*environment_guard)(void *);
 void *guard_context;
 /* Atomic fatal notification only. Never mutate shared lease here. */
 void (*fatal)(void *);
 void *fatal_context;
 uint64_t misses;
 bool bound,failed;
};
/* Caller initializes the bridge/lock/provider and freezes the shared 63-page
 * fixture/backing before owner start. Bind follows initial A publication.
 * Guard proves PAT/CD/NW/cache policy; it must not hold the bridge lock. */
bool nested_owner_dynamic_typed_bind(struct nested_owner_dynamic_typed *,struct nested_owner_sparse *);
bool nested_owner_dynamic_typed_miss(struct nested_owner_dynamic_typed *,uint64_t,uint8_t);
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
bool nested_owner_catalog_key(struct nested_owner_dynamic_typed *,uint64_t);
bool nested_owner_catalog_selected(struct nested_owner_dynamic_typed *,uint64_t);
bool nested_owner_catalog_entry(struct nested_owner_dynamic_typed *);
/* Diagnostic only: explicit L2 RO and unowned L1 witnesses from provisioning.
 * Binding/admission never assumes a table GPA is identity-mapped in L2. */
bool nested_owner_catalog_probe(struct nested_owner_dynamic_typed *,uint64_t,uint64_t);
bool nested_owner_catalog_authority(void *,const struct nrc_span *,uint64_t,struct nrc_page *);
#endif
#endif
