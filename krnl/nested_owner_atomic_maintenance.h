#ifndef SYLE_NESTED_OWNER_ATOMIC_MAINTENANCE_H
#define SYLE_NESTED_OWNER_ATOMIC_MAINTENANCE_H
#include "nested_owner_atomic.h"
#include "typed_root_atomic_maintenance.h"
/* Actual reference handoff. Only privileged primitives are injected. Stable
 * owner-local storage, fixed registered VMCS01/02 inventory. Reuse requires verified owner-local commitment. The
 * command ticket is immutable even when tam_prepare advances the live stamp.
 * Failure never removes a pin or launch state; the caller MUST retain-stop. */
struct noam_primitives {
 bool (*current)(void *,uint64_t *);
 bool (*load)(void *,uint64_t);
 bool (*read_eptp)(void *,uint64_t *);
 bool (*write_eptp)(void *,uint64_t);
 bool (*invept)(void *,uint64_t);
 bool (*parking_ready)(void *);
};
enum noam_phase { NOAM_FRESH,NOAM_BOUND,NOAM_REMOVING,NOAM_PARKED,NOAM_RESTORING,NOAM_RESTORED,NOAM_COMMITTED,NOAM_UNCERTAIN };
enum noam_failure { NOAM_OK,NOAM_GUARD,NOAM_CURRENT,NOAM_LOAD01,NOAM_READ01,NOAM_LOAD02,NOAM_READ02,NOAM_WRITE_PARK,NOAM_VERIFY_PARK,NOAM_WRITE_RESTORE,NOAM_VERIFY_RESTORE,NOAM_INVEPT };
struct nested_owner_atomic_maintenance {
 struct nested_owner_atomic *owner;
 struct tr_atomic_maintenance *maintenance;
 struct noam_primitives ops;void *context;
 struct tam_ticket ticket;
 struct nested_domain_maintenance_ticket domain_ticket;
 struct ad_entry_stamp committed_stamp;
 struct nested_ept_context_item *saved_item,*pending_item;
 uint64_t vmcs01,vmcs02,private_eptp,parking_eptp;
 uint64_t *composed_eptp;
 bool *local_launched01,*local_launched02;
 bool launched01,launched02,vcpu_launched01,vcpu_launched02;
 enum noam_phase phase;
 enum noam_failure failure;
 uint64_t observed;
};
bool noam_ticket_equal(struct tam_ticket,struct tam_ticket);
bool noam_bind(struct nested_owner_atomic_maintenance *,struct tam_ticket,struct nested_domain_maintenance_ticket);
/* Called only after the exact round release. Retires the redundant pending
 * marker, not the live hardware/source pin. No uncertain state is reusable. */
bool noam_commit(struct nested_owner_atomic_maintenance *,struct tam_ticket,struct ad_entry_stamp);
bool noam_stopped(struct nested_owner_atomic_maintenance *,struct tam_ticket);
bool noam_remove(struct nested_owner_atomic_maintenance *,struct tam_ticket);
bool noam_restore(struct nested_owner_atomic_maintenance *,struct tam_ticket,const struct tam_root *);
bool noam_flush(struct nested_owner_atomic_maintenance *,struct tam_ticket,const struct tam_root *,struct tam_flush_receipt *);
#endif
