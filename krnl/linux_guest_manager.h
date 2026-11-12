#ifndef SYLE_LINUX_GUEST_MANAGER_H
#define SYLE_LINUX_GUEST_MANAGER_H
#include "linux_guest_session.h"
/* Native exclusive BSP registration. Does not start a guest; Ring3 CREATE is
 * the first allocating operation. Requires exact native exception hook compiled
 * in and immutable linked kernel/initrd. Optional build only, not all target. */
struct lgm_service_ops {
 void *ctx;
 bool (*reset)(void *);
 bool (*dispatch)(void *,struct lgs_session *,uint32_t);
 /* Bounded queues; accepted bytes must return success despite deferred IRQ.
  * Never repeat a committed push/pop after IRQ publication failure. */
 int (*send)(void *,uint8_t);
 int (*recv)(void *);
 /* Optional pair, same lifecycle/outcome contract as lgs_platform. */
 bool (*before_entry)(void *,struct lgs_session *);
 bool (*after_entry)(void *,struct lgs_session *,const struct lgs_entry_result *);
 /* Optional after owned RAM prepare, before enable; false retains ownership. */
 bool (*prepare_guest)(void *,struct lgs_session *);
};
bool linux_guest_manager_install(const struct lgm_service_ops *);
#endif
