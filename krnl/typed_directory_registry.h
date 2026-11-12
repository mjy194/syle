#ifndef SYLE_TYPED_DIRECTORY_REGISTRY_H
#define SYLE_TYPED_DIRECTORY_REGISTRY_H
#include "typed_directory.h"
#include "nested_ept_contexts.h"
struct td_registry_bridge;
/* Provider-owned stable storage; never part of a registry item allocation.
 * Retired receipts deliberately remain live until directory teardown. */
struct td_registry_slot {
 struct td_registry_slot *next;
 struct td_registry_bridge *bridge;
 struct td_record record;
 struct td_identity identity;
 struct td_receipt receipt;
 struct td_retire_ticket retirement;
 struct nested_ept_context_item *item;
 uint64_t virtual_key;
 bool reserved,forgotten;
};
struct td_registry_provider {
 /* Success supplies exclusive initialized-zero storage and nonzero unique
  * allocator lifetime before any registry allocation. Failure owns nothing. */
 bool (*reserve)(void *,struct td_registry_slot **,uint64_t *allocator_lifetime);
 /* Guarded destroy has completed the registry's local physical flush.
  * Provider must supply receipts for every other historical CPU as needed.
  * Never infer all-history ACK from that single local flush. */
 bool (*flush_evidence)(void *,struct td_registry_slot *,struct td_retire_ticket,
                        uint64_t *acked,uint64_t *flush_serial);
};
struct td_registry_bridge {
 struct typed_directory *directory;
 struct td_registry_provider provider;
 void *provider_context;
 uint64_t owner,nonce;
 struct td_operation operation;
 enum nested_context_operation kind;
 struct td_registry_slot *slots;
 bool busy,faulted;
};
bool td_registry_bridge_init(struct td_registry_bridge *,struct typed_directory *,
 uint64_t owner,const struct td_registry_provider *,void *);
const struct nested_context_transactions *td_registry_transactions(void);
/* For external refill/entry code under a separately admitted operation.
 * The caller must use the returned stable identity, not snapshot indices.
 * Mutations remain serialized with all registry operations. */
bool td_registry_identity(struct td_registry_bridge *,void *token,
                          struct td_record **,struct td_identity *);
#endif
