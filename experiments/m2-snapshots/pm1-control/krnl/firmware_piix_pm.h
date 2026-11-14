#ifndef SYLE_FIRMWARE_PIIX_PM_H
#define SYLE_FIRMWARE_PIIX_PM_H
#include "firmware_pci_probe.h"
#include "firmware_pmtimer.h"
struct fpp_state {struct fpi_state pci;struct fpm_state pm;bool failed;};
bool fpp_init(struct fpp_state*,uint64_t epoch,uint64_t ref,uint64_t max_gap);
/* Exact limited PM config function00:01.3, no fabricated PCI header. Unknown
 * offsets at this present subset stop, never absent-all1. Old discovery reused.
 * Supports unprefixed byte/dword or 66-prefixed word forms in 32/64-bit
 * operand-default code; caller validates CS.L/CS.D, not 16-bit defaults.
 * Caller exclusive; sample effects persist, configuration/GPR publish only
 * after RIP succeeds; any failure forbids resume. Optional SCI_EN latch only; no SCI delivery, SMI or PM1 events. */
bool fpp_encoding(uint64_t qualification,unsigned length,const uint8_t*,unsigned available);
bool fpp_io(struct fpp_state*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint64_t qualification,unsigned length,uint64_t epoch,uint64_t ref,const uint8_t *instruction,unsigned available,bool exclusive,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
