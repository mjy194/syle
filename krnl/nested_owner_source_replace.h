#ifndef SYLE_NESTED_OWNER_SOURCE_REPLACE_H
#define SYLE_NESTED_OWNER_SOURCE_REPLACE_H
#include "nested_owner_atomic.h"
#include "typed_root_source_replace.h"
/* N47B: owner-local physical reference adapter. Only privileged primitives
 * and the retained parking-root audit are injected. No VMCLEAR, allocation,
 * source mutation or pin removal is authorized here. Fresh stable storage.
 * Any failure requires caller retain-stop, including after global release.
 *
 * The runtime must park BOTH owners at a complete cooperative leave, after
 * private exit reflection/nvmx_return_from_l2. Current VMCS is VMCS01. Its
 * outer gate must precede every ordinary owner config/registry read until
 * exact tsr_released AND local commit. A parked peer only polls synchronized
 * release or executes its explicit command; never call this adapter remotely.
 */
struct nosr_primitives {
 bool (*current)(void *,uint64_t *);
 bool (*load)(void *,uint64_t);
 bool (*read_eptp)(void *,uint64_t *);
 bool (*write_eptp)(void *,uint64_t);
 bool (*parking_ready)(void *);
};
struct nosr_config {
 struct nested_owner_atomic *owner;
 struct tr_source_replace *replacement;
 struct nosr_primitives ops;
 void *context;
 uint64_t *composed_eptp;
 bool *local_launched01,*local_launched02;
};
enum nosr_phase { NOSR_FRESH,NOSR_READY,NOSR_BOUND,NOSR_PARKING,NOSR_PARKED,
 NOSR_RESTORING,NOSR_RESTORED,NOSR_VALIDATED,NOSR_COMMITTED,NOSR_UNCERTAIN };
enum nosr_failure { NOSR_OK,NOSR_GUARD,NOSR_CURRENT,NOSR_LOAD01,NOSR_READ01,
 NOSR_LOAD02,NOSR_READ02,NOSR_WRITE_PARK,NOSR_VERIFY_PARK,
 NOSR_WRITE_RESTORE,NOSR_VERIFY_RESTORE };
struct nested_owner_source_replace {
 struct nosr_config config;
 /* Immutable callbacks captured before the rearm interval. Do not read
  * wrapper/atomic configuration merely to discover a closed outer gate. */
 void (*lock)(void *),(*unlock)(void *);
 unsigned (*current_cpu)(void *);
 void *metadata_context;
 unsigned cpu;
 struct tsr_ticket ticket;
 /* Stable slots and copied identities only; no retired item pointers. */
 struct tsr_root old,pending;
 struct tsr_receipt validation;
 uint64_t vmcs01,vmcs02,parking_eptp,observed;
 bool launched01,launched02,vcpu_launched01,vcpu_launched02;
 bool cleared01,cleared02,pending_live;
 enum nosr_phase phase;
 enum nosr_failure failure;
};
bool nosr_ticket_equal(struct tsr_ticket,struct tsr_ticket);
bool nosr_root_equal(struct tsr_root,struct tsr_root);
bool nosr_init(struct nested_owner_source_replace *,const struct nosr_config *);
/* Bind after exact SOURCE_REPLACE close/drain, before or in the park hook. */
bool nosr_bind(struct nested_owner_source_replace *,struct tsr_ticket);
bool nosr_stopped(struct nested_owner_source_replace *,struct tsr_ticket);
bool nosr_park(struct nested_owner_source_replace *,struct tsr_ticket,
 const struct tsr_root *,size_t,struct tsr_receipt *);
bool nosr_restore(struct nested_owner_source_replace *,struct tsr_ticket,
 const struct tsr_root *,struct tsr_receipt *);
/* Owner-local final audit, while coordinator is COMMITTING. Caller publishes
 * this exact receipt via its rendezvous before domain ack/publish/complete. */
bool nosr_validate(struct nested_owner_source_replace *,struct tsr_ticket,
 struct tsr_receipt *);
/* Only after exact final TSR release. Clears redundant pending evidence, not
 * sparse.hardware_item or its verified live source/hardware pin. */
bool nosr_commit(struct nested_owner_source_replace *,struct tsr_ticket);
/* N48 fresh adapter, both owners already parked at the next full checkpoint.
 * Observe CURRENT source-pinned hardware root, not predecessor completion's
 * historical root. No binding change, pin removal, write or allocation. */
bool nosr_checkpoint(struct nested_owner_source_replace *,struct tr_source_replace *,
 struct tsr_ticket,struct tsr_receipt *);
#endif
