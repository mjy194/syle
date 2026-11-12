#ifndef LINUX_I8042_ABSENT_H
#define LINUX_I8042_ABSENT_H
#include <stdbool.h>
#include <stdint.h>
/* Single-BSP dispatcher owns serialization and sticky failure. Callback must
 * only VMWRITE, not reenter. False may follow a partial RIP write: no resume. */
bool li8042_read(uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,
 uint64_t qualification,unsigned length,void *context,
 bool (*write)(void *,uint32_t,uint64_t));
#endif
