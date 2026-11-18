#ifndef SYLE_FIRMWARE_DISPLAY_H
#define SYLE_FIRMWARE_DISPLAY_H
#include "firmware_ramfb.h"
#include "../include/syle/boot.h"
struct fdp_stats {uint64_t frames,pixels,last_hash,last_reference;bool active;};
bool firmware_display_bind(const struct syle_framebuffer*);
void firmware_display_reset(void);
bool firmware_display_present(struct firmware_memory*,const struct frfb_state*,uint64_t reference,bool force);
struct fdp_stats firmware_display_stats(void);
void firmware_display_revoke(void);
#endif
