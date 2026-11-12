#ifndef LEGACY_IRQ_RUNTIME_H
#define LEGACY_IRQ_RUNTIME_H
#include "pit0_transaction.h"
#include "pic0_glue.h"
#include "guest_wait.h"
struct lir {struct pt0 timer;struct pgg irq;uint32_t must_one,allowed_one;bool failed,busy,inflight;};
bool lir_init(struct lir*,uint64_t domain,uint64_t primary_cap);
bool lir_before(struct lir*,const struct lei_snapshot*,void*,gw_read,gw_write);
bool lir_after(struct lir*,enum pgg_outcome,uint32_t,uint32_t,void*,gw_write);
/* Fixed object address after init: irq.pic points into timer.pic. Never copy
 * live lir; copy timer only for outer dispatch time/I/O transaction. */
#endif
