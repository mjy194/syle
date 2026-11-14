#ifndef SYLE_FIRMWARE_TIMER_IRQ_ROUTE_H
#define SYLE_FIRMWARE_TIMER_IRQ_ROUTE_H
#include "firmware_timer_irq_import.h"
/* HOST ONLY routing envelope, not protection against direct source access.
 * Caller must route EVERY source sample/config write through this owner before
 * enabling transfer. Snapshot checks detect, but cannot prevent, bypass writes.
 * No native/FCT call sites are connected by this module. Fresh private storage,
 * serialized calls, immutable source pointer/domain/epoch after bind. */
struct ftirt_route {
 struct ft100 *source;
 struct ft100 retired_snapshot;
 struct fti_import_gate import;
 struct fti_owner *owner; /* unique external owner, e.g. session.owner */
 uint64_t domain,epoch;
 bool ready,retired,failed,busy;
};
typedef enum ft100_result (*ftirt_legacy)(void*,struct ft100*,uint64_t,uint64_t);
bool ftirt_bind(struct ftirt_route*,struct ft100*,struct fti_owner*,uint64_t domain,uint64_t epoch);
bool ftirt_transfer(struct ftirt_route*,uint64_t reference,bool host_safe_quiescent);
bool ftirt_sample(struct ftirt_route*,uint64_t reference,void*,ftirt_legacy);
/* Guarded legacy entry point: forbidden once retired; must be used by all old
 * call paths. Failure is permanent; source is never sampled on that rejection. */
bool ftirt_legacy_sample(struct ftirt_route*,uint64_t reference,void*,ftirt_legacy);
#endif
