#ifndef SYLE_FIRMWARE_MANAGED_H
#define SYLE_FIRMWARE_MANAGED_H
#include <stdbool.h>
/* Fixed, BSP-only kernel registration. No allocation or guest entry until
 * Ring3 CREATE/START. The boot payload and firmware remain kernel-selected. */
bool firmware_managed_install(void);
#endif
