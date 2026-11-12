#ifndef SYLE_LINUX_PROFILE_SERVICE_H
#define SYLE_LINUX_PROFILE_SERVICE_H
#include "linux_profile_prepare.h"
#include "linux_guest_dispatch.h"
#include "linux_guest_manager.h"
struct lps_context {
 struct lpp_context prepare;struct lgd_context dispatch;struct lgd_config base;
 void *clock_ctx;bool(*now)(void*,uint64_t*);uint64_t generation;
 bool configured,busy,failed;
};
/* Singleton BSP service, stable zero object for kernel lifetime. Domain IDs
 * never repeat within that lifetime. Not safe to reset by memset after install. */
/* Clock arguments must be &context.dispatch and lgd_apic_now. */
bool lps_init(struct lps_context*,const struct lgd_config*,void*,bool(*)(void*,uint64_t*),struct lgm_service_ops*);
#endif
