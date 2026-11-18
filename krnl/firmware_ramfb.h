#ifndef SYLE_FIRMWARE_RAMFB_H
#define SYLE_FIRMWARE_RAMFB_H
#include "ramfb_config.h"
#include "firmware_fwcfg.h"
#include "firmware_memory.h"
struct frfb_state {struct ramfb_config config;uint64_t transfers,configurations;uint32_t address_high;bool active;};
/* Single stopped vCPU, immutable private ownership. Physical guest RAM only.
 * Caller stages this state and the fw_cfg cursor, commits RIP afterwards, and
 * fail-stops on every partial memory/VMCS failure. No physical DMA is issued. */
bool frfb_io(struct frfb_state*,struct ffc_state*,uint16_t port,unsigned width,
 bool input,uint64_t*rax,void*ctx,
 bool(*read)(void*,uint64_t,void*,unsigned),
 bool(*write)(void*,uint64_t,const void*,unsigned),
 bool(*owned)(void*,uint64_t,uint64_t));
#endif
