#ifndef SYLE_FIRMWARE_TIMER_IRQ_IMPORT_H
#define SYLE_FIRMWARE_TIMER_IRQ_IMPORT_H
#include "firmware_timer_irq_owner.h"
#include "firmware_timer_100m.h"
/* HOST ONLY ownership-transfer gate, fresh once per source lifetime. Caller
 * proves quiescent host-safe state: no old IRQ owner/IRR/ISR/token and no entry
 * in flight. Success permanently retires the old ft100 from all caller paths.
 * This const-source helper cannot enforce that external obligation. Making a
 * new gate or copying state is NOT permission to import the same clock again.
 * Owner domain and source epoch are distinct explicit nonzero bindings.
 * Existing terminal sources are never revived. Failure latches this gate;
 * owner/source remain unchanged. No native admission is provided. */
struct fti_import_gate {bool consumed,failed;uint64_t domain,epoch;const struct ft100*source;};
bool fti_import_once(struct fti_import_gate*,struct fti_owner*,const struct ft100*,
 uint64_t owner_domain,uint64_t expected_epoch,uint64_t reference,bool quiescent);
#endif
