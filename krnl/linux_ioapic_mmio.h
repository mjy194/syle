#ifndef LINUX_IOAPIC_MMIO_H
#define LINUX_IOAPIC_MMIO_H
#include "linux_apic_mmio.h"
#include "linux_guest_irq_adapter.h"
struct lio_state {struct gia *domain;bool ready,busy,failed;};
bool lio_init(struct lio_state *,struct gia *);
/* Opt-in, prevalidated exclusive live domain. Trusted memory/VMWRITE callbacks
 * cannot mutate devices or retain candidate pointers. Only FEC00000 page,
 * aligned32 IOREGSEL/IOWIN. No timer poll or host IO. VMwrite failure is sticky
 * FATAL, forbids resume; software APIC/IOAPIC/GIA/GPR remains unchanged.
 * RSP load writes RSP before RIP. Candidate enqueue uses cloned entire domain.
 * Success restores original long-lived callback/context before commit. */
enum lams_result lio_apply(struct lio_state*,uint64_t[15],const struct lmm_memory*,const struct lamm_exit*,uint32_t guest_edx,bool exclusive,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
