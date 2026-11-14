#ifndef SYLE_FIRMWARE_LINT0_QUIET_H
#define SYLE_FIRMWARE_LINT0_QUIET_H
#include "linux_pic_mask.h"
/* PIC mask-only admission for a separate source-free LINT0 ExtINT profile.
 * This neither proves absence of producers nor drains events. Caller must
 * independently exclude all PIC input producers, pending external events and
 * concurrent mutation for the entire admission/entry interval. No host IO,
 * IRQ injection or NMI behavior. Does not inspect or rely on guest IF=0.
 * Pure, nonmutating predicate; completed ICW or cold reset masks only. */
bool flq_pic_quiet(const struct lpic_state*);
#endif
