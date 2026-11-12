#ifndef SYLE_MANAGEMENT_GUEST_H
#define SYLE_MANAGEMENT_GUEST_H
#include "../../include/syle/guest_control.h"
/* For the Syle Ring3 manager only, never invoke from a host test process. */
static inline int64_t syle_guest_call(uint64_t operation, uint64_t handle,
                                    uint64_t argument) {
    uint64_t result=operation;
    __asm__ volatile("int $0x80" : "+a"(result) : "D"(handle), "S"(argument)
                     : "memory", "cc");
    return (int64_t)result;
}
#endif
