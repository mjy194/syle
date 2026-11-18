#ifndef SYLE_FIRMWARE_DISK_H
#define SYLE_FIRMWARE_DISK_H
#include <stdbool.h>
#include <stddef.h>
/* Boot-owned immutable bytes, disjoint from guest allocation, kept reserved
 * until the vCPU/device is fully stopped. No guest pointers accepted here. */
bool firmware_disk_bind(const void*,size_t);
#endif
