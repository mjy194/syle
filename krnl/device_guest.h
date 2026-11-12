#ifndef SYLE_DEVICE_GUEST_H
#define SYLE_DEVICE_GUEST_H
#include "ept.h"
#include "vmx_guest.h"
/* Check full GPA before any narrowing/indexing into private backing. */
static inline bool device_guest_ram_gpa(uint64_t gpa) {
    return gpa>=4096 && gpa<((uint64_t)EPT_GUEST_PAGES+1)*4096;
}
static inline bool device_mmio_state_supported(uint64_t blocking,uint64_t flags,
                                                uint64_t pending_debug) {
    return !blocking && !(flags & (UINT64_C(1)<<8)) && !pending_debug;
}
bool device_guest_init(void);
/* -1 stop failure, 0 resume, 1 done. VMCS writes fail closed. */
int device_guest_exit(struct ept_fixture *f,uint32_t primary,uint64_t reason,
                      uint64_t rip,uint64_t rsp);
#endif
