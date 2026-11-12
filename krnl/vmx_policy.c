#include "vmx.h"
const char *vmx_gate(const struct vmx_caps *c) {
    if (!(c->feature_control & 1)) return "FEATURE_CONTROL_UNLOCKED";
    if (!(c->feature_control & 4)) return "FIRMWARE_DISABLED";
    unsigned size=(unsigned)((c->basic>>32)&0x1fff);
    if (!size || size>4096 || (c->basic & (1ULL<<31))) return "INVALID_BASIC";
    if (((c->basic>>50)&15)!=6) return "UNSUPPORTED_MEMORY_TYPE";
    if (c->cr4 & (1ULL<<13)) return "ALREADY_VMXE";
    if ((c->cr0 & c->cr0_fixed0)!=c->cr0_fixed0 ||
        (c->cr0 & ~c->cr0_fixed1)) return "CR0_FIXED_BITS";
    uint64_t proposed=c->cr4 | (1ULL<<13);
    if ((proposed & c->cr4_fixed0)!=c->cr4_fixed0 ||
        (proposed & ~c->cr4_fixed1)) return "CR4_FIXED_BITS";
    return 0;
}
