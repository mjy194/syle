#ifndef SYLE_FIRMWARE_FWCFG_TRANSACTION_H
#define SYLE_FIRMWARE_FWCFG_TRANSACTION_H
#include "firmware_fwcfg.h"
#include "firmware_fwcfg_target.h"
struct fwc_state {struct ffc_state stream;bool failed;unsigned stage;};
struct fwc_exit {uint64_t rip,qualification,flags,blocking,pending_debug,dr7,entry_event,vectoring,es_base,linear;uint32_t length,info,es_ar;bool info_supported;};
/* Exclusive stopped vCPU. Partial RAM/VMCS writes are fatal, never rollback
 * claims; caller destroys session. GPR/cursor publish only after full success. */
bool fwc_apply(struct fwc_state*,uint64_t[15],const struct fmm_memory*,const struct fwc_exit*,bool exclusive,void*,bool(*ram_write)(void*,uint8_t*,const uint8_t*,unsigned),bool(*vmwrite)(void*,uint32_t,uint64_t));
#endif
