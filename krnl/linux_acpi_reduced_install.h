#ifndef LINUX_ACPI_REDUCED_INSTALL_H
#define LINUX_ACPI_REDUCED_INSTALL_H
#include "linux_acpi_reduced.h"
#include "linux_acpi_install.h"
/* Separate optional full-description installation; old fragment/default unchanged.
 * Owned-GPA callbacks only, single-BSP immutable RAM, before any guest execution.
 * False+LAI_DIRTY forbids entry/retry and requires owner destroy/recreate. */
bool lar_install(enum lai_state*,const struct lai_ops*,uint64_t ram_size,uint64_t params_gpa);
/* Gate calls this directly against actual RAM, never trusts install state/report.
 * Requires read callback only; no writes. Exact fixed generator bytes, not merely
 * checksum or magic validation. Caller must prevent RAM mutation until use. */
bool lar_verify_owned(const struct lai_ops*,uint64_t ram_size,uint64_t params_gpa);
#endif
