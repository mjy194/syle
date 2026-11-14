#ifndef SYLE_FIRMWARE_TIMER_IRQ_SESSION_IMPORT_H
#define SYLE_FIRMWARE_TIMER_IRQ_SESSION_IMPORT_H
#include "firmware_timer_irq_session.h"
#include "firmware_timer_irq_route.h"
/* HOST ONLY. Fresh zeroed session AND route, stable shared lifetime with live
 * source. Route binds directly to session.owner. Success retires source under
 * route's external call-site obligations; no ftis_init/fti_init/restart/copy.
 * No native VMCS identity proof is supplied. Failure permanently poisons the
 * session/route, never sets session.ready; neither may be reused or reset.
 * Existing after() continues to use the unchanged lei whitelist. */
bool ftis_import(struct ftis_session*,struct ftirt_route*,struct ft100*,
 uint64_t domain,uint64_t vmcs,uint64_t epoch,uint64_t reference,bool host_safe);
#endif
