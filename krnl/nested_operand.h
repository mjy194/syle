#ifndef SYLE_NESTED_OPERAND_H
#define SYLE_NESTED_OPERAND_H
#include <stddef.h>
#include "nested_vmx.h"
struct nested_operand {
    enum nvmx_op op;
    uint64_t ea;
    uint8_t length,regA,regB;
    bool memory;
};
/* Restricted long-mode decoder, 48-bit canonical addresses, no wrapping EA
 * arithmetic, no cross-page m64/m128. GPRs are architectural encoding order.
 * regA = READ destination / WRITE value (ModRM r/m); regB = field (reg).
 * INVEPT returns regB = type register and a 16-byte memory operand;
 * decoder does not require alignment (an adapter may impose stricter scope).
 * Unused reg indices = 0xff. Failure leaves output untouched. This does not
 * translate memory or implement privilege/fault ordering. */
bool nested_decode_operand(const uint8_t *code,size_t available,uint64_t rip,
                           const uint64_t gpr[16],uint32_t expected_reason,
                           struct nested_operand *out);

/* Cross-check Intel VM-exit memory instruction info. Undefined bits ignored;
 * qualification already includes next RIP for RIP-relative operands. */
static inline bool nested_memory_exit_matches(uint64_t info,uint64_t qualification,
    const uint64_t gpr[16],uint64_t decoded_ea) {
    if(((info>>7)&7)!=2) return false; /* 64-bit address size */
    unsigned segment=(info>>15)&7;
    if(segment!=2&&segment!=3) return false; /* flat SS/DS only */
    uint64_t ea=qualification;
    if(!(info&(UINT64_C(1)<<27))) ea+=gpr[(info>>23)&15];
    if(!(info&(UINT64_C(1)<<22))) ea+=gpr[(info>>18)&15]<<(info&3);
    return ea==decoded_ea;
}

#endif
