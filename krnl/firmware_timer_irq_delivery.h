#ifndef SYLE_FIRMWARE_TIMER_IRQ_DELIVERY_H
#define SYLE_FIRMWARE_TIMER_IRQ_DELIVERY_H
#include "firmware_timer_irq_session.h"
#include "firmware_irq_native_evidence.h"
/* Independent HOST-ONLY opt-in after-entry bridge. Full evidence is bound to
 * current session + staged token, then fine_classify decides. ACCEPTED means
 * vector delivery phase crossed, NOT handler execution/completion. Native must
 * still produce truthful evidence from the actual exclusive assembly attempt.
 * No old lei/ftis_after whitelist is modified. No native call site is wired. */
enum ftis_result ftid_after(struct ftis_session*,const struct fine_outcome*,void*,lei_write);
#endif
