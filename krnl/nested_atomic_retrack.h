#ifndef SYLE_NESTED_ATOMIC_RETRACK_H
#define SYLE_NESTED_ATOMIC_RETRACK_H
#include "nested_owner_atomic_maintenance.h"
/* One immutable-source, two-owner round. A single writer for each owner and
 * BSP-only coordinator. Commands are immutable after release publication and
 * never overwritten until both matching acquire acknowledgments. No VM entry
 * is permitted in these calls. Poll is a bounded healthy/timeout check, not
 * permission to enter; NULL uses the bounded iteration count alone. Poll
 * also runs under the directory metadata lock at final publication: it must
 * be nonblocking and must never acquire/re-enter that metadata lock. */
#define NART_POLLS 268435456u
enum nart_command { NART_NONE,NART_BIND,NART_REVOKE,NART_RESET,NART_PREPARE,NART_RELEASE };
struct nart_evidence {
 struct tam_ticket ticket;
 struct nested_domain_maintenance_ticket domain_ticket;
 struct nested_domain_token token;
 unsigned cpu;
};
struct nested_atomic_retrack {
 struct tr_atomic_maintenance maintenance;
 struct nested_owner_atomic_maintenance refs[2];
 struct nested_domain_dispatch *dispatch;
 struct ad_entry_stamp *outer_stamp;
 bool (*poll)(void *);void *context;
 uint32_t empty[2],bootstrapped,arrived[2],bound[2],ack[2],command,fault,released;
 struct ad_entry_stamp arrival_stamp[2];
 struct nart_evidence evidence[2];
 struct tam_ticket ticket;
 struct nested_domain_maintenance_ticket domain_ticket;
};
/* Install as owner.empty_ready after configuring both refs/dispatch/stamp. */
bool nart_empty_ready(struct nested_owner_atomic *,void *);
/* Called by BOTH stopped owners at the phase-2 checkpoint. Captures arrivals
 * before tam_close can close normal admission needed by a lagging owner. */
bool nart_round(struct nested_atomic_retrack *,unsigned cpu);
bool nart_failed(const struct nested_atomic_retrack *);
#endif
