#ifndef GUEST_WAIT_H
#define GUEST_WAIT_H
#include <stdbool.h>
#include <stdint.h>
typedef bool (*gw_read)(void *,uint32_t,uint64_t *);
typedef bool (*gw_write)(void *,uint32_t,uint64_t);
/* Every false requires caller's sticky no-resume, including uncertain writes. */
bool gw_halt(void *,gw_read,gw_write);
bool gw_window(void *,gw_read,gw_write,uint32_t must_one,uint32_t allowed_one,bool enable);
bool gw_window_exit(void *,gw_read,gw_write,uint32_t must_one,uint32_t allowed_one);
#endif
