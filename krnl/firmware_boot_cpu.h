#ifndef SYLE_FIRMWARE_BOOT_CPU_H
#define SYLE_FIRMWARE_BOOT_CPU_H
#include "firmware_active_cpu_tsc.h"
#include <stddef.h>
/* One fixed reset-through-Linux CPU contract; host capability checked. */
bool fboot_query(const struct fcp_host*,bool,uint32_t,uint32_t,struct fcp_result*);
bool fboot_efer_allowed(uint64_t);
bool fboot_cr_encoding(uint64_t,unsigned,const uint8_t*,size_t);
bool fboot_preemption_resume(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
#endif
