#ifndef SYLE_FIRMWARE_VCPU_PLAN_H
#define SYLE_FIRMWARE_VCPU_PLAN_H
#include "firmware_layout.h"
#include "linux_vcpu_prepare.h" /* shared capability/field containers only */
/* Pure reset-state plan. Requires EPT + unrestricted guest + WBINVD exits.
 * Caller owns/mapped every backing page and supplies an EPTP with 4-level WB
 * flags, no A/D. This API cannot validate backing ownership or EPT contents.
 * Host fields and runtime CR/EFER emulation are NOT provided by this module.
 * Do not use with the Linux-only dispatcher or start a partially wired VM.
 * Reset CS base FFFF0000 and RIP FFF0 yield linear reset address FFFFFFF0.
 * CR0.NE and CR4.VMXE may be hardware-only; read shadows expose reset values.
 * No guest memory is written. Output unchanged on failure. */
bool fvp_reset_plan(const struct fwl_request*,uint64_t eptp,const struct lvp_caps*,struct lvp_plan*);
#endif
