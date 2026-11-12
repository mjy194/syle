#ifndef LINUX_ACPI_INSTALL_H
#define LINUX_ACPI_INSTALL_H
#include "linux_acpi_topology.h"
enum lai_state { LAI_FRESH, LAI_DIRTY, LAI_INSTALLED };
struct lai_ops {void *context; bool (*read)(void *,uint64_t,void *,size_t); bool (*write)(void *,uint64_t,const void *,size_t);};
/* Zero-initialized state; trusted owned-GPA callbacks, immutable single-BSP
 * guest memory before first entry. No raw GPA-to-host pointer conversion.
 * False+FRESH means no writes; false+DIRTY forbids boot/retry and requires guest
 * destruction/recreation through its owner. Never frees memory here.
 * Success only installs a topology fragment, not complete ACPI firmware.
 * Opt-in only; no default loader/session call site. */
bool lai_install(enum lai_state *,const struct lai_ops *,uint64_t ram_size,uint64_t params_gpa);
#endif
