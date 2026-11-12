#include "lapic.h"

#define APIC_BASE_XAPIC UINT64_C(0xfee00900)
#define APIC_BASE_X2APIC UINT64_C(0xfee00d00)

static int highest(const uint32_t bitmap[8]) {
    for (int vector = 255; vector >= 0; --vector)
        if (bitmap[(unsigned)vector / 32] & (UINT32_C(1) << (vector % 32)))
            return vector;
    return -1;
}
static bool enabled(const struct lapic *a) {
    return a->base == APIC_BASE_X2APIC && (a->svr & 0x100) != 0;
}
void lapic_init(struct lapic *a) {
    *a = (struct lapic){.base = APIC_BASE_XAPIC, .svr = 0xff};
}
bool lapic_enqueue_external(struct lapic *a, unsigned vector) {
    if(!a || !enabled(a) || vector<32 || vector>255) return false;
    a->irr[vector/32] |= UINT32_C(1) << (vector%32);
    return true;
}
bool lapic_eoi_vector(struct lapic *a, uint8_t *vector) {
    if(!a || !vector || a->base!=APIC_BASE_X2APIC) return false;
    int retired=highest(a->isr);
    if(retired<0) return false;
    a->isr[(unsigned)retired/32] &= ~(UINT32_C(1) << (retired%32));
    *vector=(uint8_t)retired;
    return true;
}
uint8_t lapic_ppr(const struct lapic *a) {
    int in_service = highest(a->isr);
    unsigned isr_class = in_service < 0 ? 0 : (unsigned)in_service & 0xf0u;
    /* Equal priority class selects the entire TPR, including its low nibble.
     * If ISR wins, PPR's low nibble is zero. */
    return (a->tpr & 0xf0u) >= isr_class ? a->tpr : (uint8_t)isr_class;
}
bool lapic_read(const struct lapic *a, uint32_t msr, uint64_t *value) {
    uint64_t result;
    if (msr == 0x1b) {
        *value = a->base;
        return true;
    }
    if (a->base != APIC_BASE_X2APIC) return false;
    if (msr >= 0x810 && msr <= 0x817) result = a->isr[msr - 0x810];
    else if (msr >= 0x820 && msr <= 0x827) result = a->irr[msr - 0x820];
    else switch (msr) {
    case 0x802: result = 0; break; /* fixed single-vCPU APIC ID */
    case 0x803: result = 0x14; break; /* diagnostic version, not LVT support */
    case 0x808: result = a->tpr; break;
    case 0x80a: result = lapic_ppr(a); break;
    case 0x80f: result = a->svr; break;
    default: return false; /* EOI and self-IPI are write-only */
    }
    *value = result;
    return true;
}
bool lapic_write(struct lapic *a, uint32_t msr, uint64_t value) {
    if (msr == 0x1b) {
        /* Fixed base and BSP identity. Allow idempotence and only the one
         * supported mode transition; disable/back-transition are rejected. */
        if (value != a->base &&
            !(a->base == APIC_BASE_XAPIC && value == APIC_BASE_X2APIC))
            return false;
        a->base = value;
        return true;
    }
    if (a->base != APIC_BASE_X2APIC) return false;
    switch (msr) {
    case 0x808:
        if (value > 0xff) return false;
        a->tpr = (uint8_t)value;
        return true;
    case 0x80f:
        /* Only vector and software-enable supported. Suppression/focus and
         * reserved fields are not silently accepted. */
        if ((value & ~UINT64_C(0x1ff)) || (value & 0xff) < 32) return false;
        a->svr = (uint16_t)value;
        return true;
    case 0x80b: {
        if (value != 0) return false;
        uint8_t vector;
        (void)lapic_eoi_vector(a,&vector);
        return true; /* EOI with no in-service vector is a no-op */
    }
    case 0x83f:
        if (!enabled(a) || value < 32 || value > 255) return false;
        a->irr[value / 32] |= UINT32_C(1) << (value % 32);
        return true; /* repeated pending same vector coalesces */
    default:
        return false;
    }
}
bool lapic_select(const struct lapic *a, uint8_t *vector) {
    if (!enabled(a)) return false;
    int pending = highest(a->irr);
    if (pending < 0 || ((unsigned)pending & 0xf0u) <= (lapic_ppr(a) & 0xf0u))
        return false;
    *vector = (uint8_t)pending;
    return true;
}
bool lapic_accept(struct lapic *a, uint8_t vector) {
    uint8_t selected;
    if (!lapic_select(a, &selected) || selected != vector) return false;
    uint32_t bit = UINT32_C(1) << (vector % 32);
    a->irr[vector / 32] &= ~bit;
    a->isr[vector / 32] |= bit;
    return true;
}

bool lapic_window_caps(uint64_t caps) {
    return !(caps & 4) && ((caps >> 32) & 4);
}
bool lapic_window_arm(const struct lapic *a, bool armed, bool injected,
    uint64_t pending, uint64_t activity) {
    uint8_t vector;
    return !armed && !injected && !(pending & (1ULL<<31)) &&
           activity==0 && lapic_select(a,&vector);
}
bool lapic_window_ready(const struct lapic *a, uint64_t reason, uint64_t flags,
    uint64_t blocking, uint64_t activity, uint64_t pending, uint64_t vectoring,
    uint8_t *vector) {
    return reason==7 && (flags & (1u<<9)) && !blocking && !activity &&
           !(pending & (1ULL<<31)) && !(vectoring & (1ULL<<31)) &&
           lapic_select(a,vector);
}

/* Intel SDM Vol3C 326019-084US §§27.3.1,27.7.1:
 * type3 exception may vector with guest MOVSS blocking; vectoring consumes
 * shadow architecturally. Never relax external-interrupt window policy. */
bool lapic_collision_ud_ready(const struct lapic *a, uint64_t reason,
    uint64_t info,uint64_t blocking,uint64_t flags,uint64_t activity,
    uint64_t pending,uint64_t vectoring,bool already_injected) {
    uint8_t selected;
    return !already_injected && reason==0 && info==UINT64_C(0x80000306) &&
           blocking==2 && (flags&(1u<<9)) && !(flags&(1u<<8)) && !activity &&
           !(pending&(1ULL<<31)) && !(vectoring&(1ULL<<31)) &&
           lapic_select(a,&selected) && selected==0x40;
}

bool lapic_irq_ack_valid(uint64_t reason, uint64_t rip, uint64_t first_handler_rip,
                         uint64_t vectoring) {
    return reason==31 && rip==first_handler_rip && !(vectoring&(UINT64_C(1)<<31));
}
