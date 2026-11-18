#ifndef SYLE_FIRMWARE_BOOT_INVLPG_H
#define SYLE_FIRMWARE_BOOT_INVLPG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool fbinv_decode(const uint8_t*,size_t,unsigned,const uint64_t[16],uint64_t);
#endif
