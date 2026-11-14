#ifndef SYLE_PCI_CONFIG_PROJECTION_H
#define SYLE_PCI_CONFIG_PROJECTION_H
#include "../../krnl/firmware_pmtimer.h"
/* Isolated diagnostic profile; caller owns zeroed metadata + same live PM state
 * as fpp. Nonalias inputs and exclusive no-reentry access required. PM decode
 * uses fixed PMBA/PMIOSE, not PCI command.IO (PIIX4 legacy port semantics). */
struct vpc {uint16_t pm_command;};
/* -1 rejected/no change, 0 BDF not ours/no change, 1 handled. */
int vpc_config(struct vpc*,struct fpm_state*,uint16_t,unsigned,unsigned,bool,uint32_t*);
bool vpc_pm_decode(const struct vpc*,const struct fpm_state*);
#endif
