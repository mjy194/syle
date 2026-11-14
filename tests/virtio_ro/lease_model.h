#ifndef SYLE_HOST_LEASE_MODEL_H
#define SYLE_HOST_LEASE_MODEL_H
#include <stdint.h>
#include <stdbool.h>
#ifndef VLM_SLOTS
#define VLM_SLOTS 32
#endif
/* Host interface experiment only, not authoritative native PMM integration.
 * Fresh zero-initialized, stable storage required; init may succeed once only.
 * Serial caller, no external free/map/share; no adoption of caller PA tokens.
 * Handles are valid only with the exact issuing manager, which must outlive all
 * handles. Storage must not be copied, zeroed/reused or reinitialized. */
/* release(false) MUST leave allocation owned and unchanged, as real PMM does. */
struct vlm_ops {void *ctx;uint64_t (*allocate)(void *);bool (*release)(void *,uint64_t);};
enum vlm_kind {VLM_UNSPECIFIED,VLM_GUEST_RAM,VLM_EPT_TABLE,VLM_VMX_CONTROL,VLM_HOST_METADATA};
struct vlm_slot {uint64_t pa,owner,generation,lease;enum vlm_kind kind;};
struct vlm {struct vlm_ops ops;struct vlm_slot slot[VLM_SLOTS];uint64_t serial;bool initialized,draining,failed;};
struct vlm_allocation {unsigned slot;uint64_t generation;const struct vlm *issuer;};
struct vlm_handle {unsigned slot;uint64_t serial;const struct vlm *issuer;};
bool vlm_init(struct vlm *,struct vlm_ops);
bool vlm_allocate_typed(struct vlm *,uint64_t owner,enum vlm_kind,struct vlm_allocation *);
bool vlm_kind_of(const struct vlm *,uint64_t owner,struct vlm_allocation,enum vlm_kind *);
bool vlm_allocate(struct vlm *,uint64_t owner,struct vlm_allocation *);
bool vlm_allocation_page(const struct vlm *,uint64_t owner,struct vlm_allocation,uint64_t *);
bool vlm_pin(struct vlm *,uint64_t owner,struct vlm_allocation,struct vlm_handle *);
bool vlm_unpin(struct vlm *,struct vlm_handle);
bool vlm_release(struct vlm *,uint64_t owner,struct vlm_allocation);
void vlm_begin_drain(struct vlm *);
bool vlm_drained(const struct vlm *);
#endif
