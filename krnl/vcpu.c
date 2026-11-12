#include "vcpu.h"
void vcpu_fault(struct vcpu *v) { v->state=VCPU_FAULTED; }
bool vcpu_inject_ud(struct vcpu *v, uint64_t reason, uint64_t info,
    uint64_t vectoring, uint64_t pending, uint64_t blocking, uint64_t activity,
    uint64_t rip, uint64_t expected, uint32_t *entry_info) {
    /* Exact interrupt info rejects error-code, NMI-unblocking/reserved bits.
     * #UD is a fault: hardware exception type 3, no error code, same RIP.
     * IF is irrelevant to synchronous #UD; this fixture accepts no blocking. */
    if(v->state!=VCPU_RUNNABLE || v->ud_injected || reason!=0 ||
       info!=UINT64_C(0x80000306) || (vectoring&(1ULL<<31)) ||
       (pending&(1ULL<<31)) || blocking || activity || rip!=expected) {
        vcpu_fault(v); return false;
    }
    *entry_info=0x80000306u;
    v->ud_injected=true;
    return true;
}
bool vcpu_ack_ud(struct vcpu *v) {
    if(v->state!=VCPU_RUNNABLE || !v->ud_injected || v->ud_handled) {
        vcpu_fault(v); return false;
    }
    v->ud_handled=true; return true;
}
bool vcpu_halt(struct vcpu *v) {
    if(v->state!=VCPU_RUNNABLE || !v->ud_handled) {
        vcpu_fault(v); return false;
    }
    v->state=VCPU_HALTED; return true;
}
bool vcpu_complete(struct vcpu *v) {
    if(v->state!=VCPU_HALTED) { vcpu_fault(v); return false; }
    v->state=VCPU_COMPLETED; return true;
}

bool vcpu_inject_irq(struct vcpu *v, uint32_t vector, uint64_t flags,
    uint64_t blocking, uint64_t activity, uint64_t pending, uint64_t vectoring,
    uint32_t *entry_info) {
    if(v->state!=VCPU_HALTED || !v->ud_handled || v->irq_pending || v->irq_handled ||
       vector<32 || vector>255 || !(flags&(1u<<9)) || blocking || activity!=1 ||
       (pending&(1ULL<<31)) || (vectoring&(1ULL<<31))) {
        vcpu_fault(v);return false;
    }
    *entry_info=0x80000000u|vector; /* external interrupt type 0, no error code */
    v->irq_pending=true; /* state remains HALTED until hardware wake is proven */
    return true;
}
bool vcpu_ack_irq(struct vcpu *v, uint64_t activity) {
    if(v->state!=VCPU_HALTED || !v->irq_pending || v->irq_handled || activity!=0) {
        vcpu_fault(v);return false;
    }
    v->irq_pending=false;v->irq_handled=true;v->state=VCPU_RUNNABLE;return true;
}

bool vcpu_inject_gp(struct vcpu *v, uint64_t reason, uint64_t info, uint64_t error,
    uint64_t vectoring, uint64_t pending, uint64_t blocking, uint64_t activity,
    uint64_t rip, uint64_t expected, uint32_t *entry_info, uint32_t *entry_error) {
    if(v->state!=VCPU_RUNNABLE || !v->ud_handled || v->gp_pending || v->gp_handled ||
       reason!=0 || info!=UINT64_C(0x80000b0d) || error!=0x28 ||
       (vectoring&(1ULL<<31)) || (pending&(1ULL<<31)) || blocking || activity ||
       rip!=expected) {
        vcpu_fault(v); return false;
    }
    *entry_info=0x80000b0du;*entry_error=(uint32_t)error;
    v->gp_pending=true;return true;
}
bool vcpu_ack_gp(struct vcpu *v) {
    if(v->state!=VCPU_RUNNABLE || !v->gp_pending || v->gp_handled) {
        vcpu_fault(v);return false;
    }
    v->gp_pending=false;v->gp_handled=true;return true;
}
bool vcpu_inject_pf(struct vcpu *v, uint64_t reason, uint64_t info, uint64_t error,
    uint64_t gla, uint64_t vectoring, uint64_t pending, uint64_t blocking, uint64_t activity,
    uint64_t rip, uint64_t expected, uint32_t *entry_info, uint32_t *entry_error) {
    if(v->state!=VCPU_RUNNABLE || !v->ud_handled || v->pf_pending || v->pf_handled ||
       reason!=0 || info!=UINT64_C(0x80000b0e) || error!=0 || gla!=0xc000 ||
       (vectoring&(1ULL<<31)) || (pending&(1ULL<<31)) || blocking || activity ||
       rip!=expected) {
        vcpu_fault(v); return false;
    }
    *entry_info=0x80000b0eu;*entry_error=(uint32_t)error;
    v->pf_pending=true;return true;
}
bool vcpu_ack_pf(struct vcpu *v) {
    if(v->state!=VCPU_RUNNABLE || !v->pf_pending || v->pf_handled) {
        vcpu_fault(v);return false;
    }
    v->pf_pending=false;v->pf_handled=true;return true;
}
