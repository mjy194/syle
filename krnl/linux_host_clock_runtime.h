#ifndef SYLE_LINUX_HOST_CLOCK_RUNTIME_H
#define SYLE_LINUX_HOST_CLOCK_RUNTIME_H
#include "linux_host_clock.h"
#include "linux_pic04_domain.h"
struct lhcr_source {struct lhc_source properties;uint64_t max_gap;void*ctx;bool(*sample)(void*,struct lhc_sample*);};
/* No changes on false. Caller latches failure; reserved-ticket busy is checked
 * before sampling by the external-I/O adapter, never hidden by this helper. */
bool lhcr_init(struct lhc_clock*,const struct lhcr_source*);
bool lhcr_start(struct lhc_clock*,const struct lhcr_source*);
bool lhcr_sync(struct lhc_clock*,struct lpd_domain*,const struct lhcr_source*,bool stop);
bool lhcr_stage(const struct lhc_clock*,const struct lhcr_source*,struct lhc_clock*);
#endif
