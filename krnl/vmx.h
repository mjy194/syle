#ifndef SYLE_VMX_H
#define SYLE_VMX_H
#include <stdint.h>
struct vmx_caps {
    uint64_t feature_control, basic, cr0_fixed0, cr0_fixed1;
    uint64_t cr4_fixed0, cr4_fixed1, cr0, cr4;
};
/* Pure gate, also exercised by host unit tests. Never enables firmware policy. */
const char *vmx_gate(const struct vmx_caps *c);
void vmx_probe(void);
#endif
