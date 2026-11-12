#ifndef SYLE_LINUX_APIC_DISPATCH_H
#define SYLE_LINUX_APIC_DISPATCH_H
#include "linux_apic_mmio.h"
#include "linux_guest_irq_adapter.h"
#include "linux_ioapic_mmio.h"
struct lgs_session;
struct lad_config {
 struct lams_state *transactions;
 struct gia *irq;
 void *clock_ctx;
 bool (*now)(void*,uint64_t*); /* explicit synthetic device time */
 struct lio_state *ioapic;
};
/* Every pointer belongs to this guest domain; never combine legacy lia with
 * guest_apic. Does not initialize, enable or advertise devices. */
bool lad_valid(const struct lad_config*);
enum lams_result lad_mmio(const struct lad_config*,struct lgs_session*,uint32_t guest_edx);
enum lams_result lad_msr(const struct lad_config*,struct lgs_session*,uint32_t guest_ecx,uint32_t guest_edx);
#endif
