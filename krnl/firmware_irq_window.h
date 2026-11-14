#ifndef SYLE_FIRMWARE_IRQ_WINDOW_H
#define SYLE_FIRMWARE_IRQ_WINDOW_H
#include "linux_external_irq.h"
struct fiw_state {uint64_t domain,vmcs,generation,attempt;uint32_t base;bool ready,armed,failed,busy;};
enum fiw_result {FIW_FATAL=-1,FIW_WAIT,FIW_READY,FIW_ARMED,FIW_RECHECK};
/* Host-only. Actual identities/controls/capabilities are read by native caller.
 * Window is NOT IRQ reservation/delivery. PPR-blocked WAIT still permits only
 * bounded guest execution; EOI/TPR changes or preemption must trigger recheck. */
bool fiw_init(struct fiw_state*,uint64_t,uint64_t,uint32_t,uint64_t capabilities);
enum fiw_result fiw_before(struct fiw_state*,uint64_t,uint64_t,uint32_t actual,
 bool pending,bool priority_ready,const struct lei_snapshot*,void*,lei_write,uint64_t*attempt);
/* Called at host-safe exit BEFORE dispatch. Restore entire saved controls,
 * no RIP/RFLAGS write. Any exit may end the window attempt, not only reason7.
 * Caller must quarantine failures/unknown native outcomes, never resume. */
enum fiw_result fiw_after(struct fiw_state*,uint64_t,uint64_t,uint64_t attempt,
 uint32_t actual,uint32_t reason,bool real_host_safe_exit,void*,lei_write);
#endif
