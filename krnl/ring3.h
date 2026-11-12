#ifndef SYLE_RING3_H
#define SYLE_RING3_H
#include "../include/syle/boot.h"
#include <stdbool.h>
struct guest_backend;
/* Kernel-only registration before entering the bootstrap manager. */
bool ring3_guest_bind(const struct guest_backend *, void *context);
__attribute__((noreturn)) void ring3_start(const struct syle_boot_info *boot);
#endif
