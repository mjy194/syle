#ifndef LINUX_PIC04_RUNTIME_H
#define LINUX_PIC04_RUNTIME_H
#include "pic_master_04_glue.h"
#include "guest_wait.h"
/* Caller must authenticate exact PF/GP/BP owner, entry/error/length and
 * vectoring before before(): it writes the window control first. False after
 * any VMCS write forbids resume; no partial VMCS rollback is promised.
 * after UNKNOWN always fails even if no ticket existed. */
struct lpr_runtime {struct pmg irq;uint32_t must_one,allowed_one;bool ready,failed,busy,inflight;};
bool lpr_init(struct lpr_runtime*,const struct pic_master_04*,uint64_t);
bool lpr_before(struct lpr_runtime*,struct pic_master_04*,const struct lei_snapshot*,void*,gw_read,gw_write);
bool lpr_after(struct lpr_runtime*,struct pic_master_04*,enum pmg_outcome,uint32_t,uint32_t,void*,gw_write);
#endif
