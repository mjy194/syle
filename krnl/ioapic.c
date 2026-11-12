#include "ioapic.h"
#define MASK (1u<<16)
#define LOW_ACTIVE (1u<<13)
#define LEVEL (1u<<15)
#define REMOTE (1u<<14)
static bool deliver(struct ioapic *a,struct ioapic_pin *p) {
    if(!a->enqueue || !a->enqueue(a->context,(uint8_t)p->low)) return false;
    if(p->low&LEVEL) p->remote_irr=true;
    return true;
}
static bool level_ready(const struct ioapic_pin *p) {
    return (p->low&LEVEL) && !(p->low&MASK) && p->asserted && !p->remote_irr;
}
void ioapic_init(struct ioapic *a,ioapic_enqueue_fn enqueue,void *context) {
    *a=(struct ioapic){.enqueue=enqueue,.context=context};
    for(unsigned i=0;i<IOAPIC_PINS;i++) a->pins[i].low=MASK;
}
void ioapic_init_guest(struct ioapic *a,ioapic_enqueue_fn enqueue,void *context) {
    ioapic_init(a,enqueue,context);
    a->guest_profile=true;
}
bool ioapic_set_level(struct ioapic *a,unsigned pin,bool high) {
    if(!a || pin>=IOAPIC_PINS) return false;
    bool asserted=high != ((a->pins[pin].low&LOW_ACTIVE)!=0);
    return ioapic_set_irq(a,pin,asserted);
}
bool ioapic_retry(struct ioapic *a,unsigned pin) {
    if(pin>=IOAPIC_PINS) return false;
    return !level_ready(&a->pins[pin]) || deliver(a,&a->pins[pin]);
}
bool ioapic_set_irq(struct ioapic *a,unsigned pin,bool asserted) {
    if(pin>=IOAPIC_PINS) return false;
    struct ioapic_pin *p=&a->pins[pin];
    bool rising=asserted&&!p->asserted;
    p->asserted=asserted;
    if(p->low&LEVEL) return ioapic_retry(a,pin);
    if(rising && !(p->low&MASK)) return deliver(a,p);
    return true;
}
bool ioapic_eoi(struct ioapic *a,uint8_t vector) {
    bool ok=true;
    for(unsigned i=0;i<IOAPIC_PINS;i++) {
        struct ioapic_pin *p=&a->pins[i];
        if(p->remote_irr && (uint8_t)p->low==vector) {
            p->remote_irr=false;
            if(!ioapic_retry(a,i)) ok=false;
        }
    }
    return ok;
}
enum device_status ioapic_mmio(void *context,uint64_t offset,unsigned width,
    bool write,uint32_t *value) {
    struct ioapic *a=context;
    if(!a || !value || width!=4 || (offset&3)) return DEVICE_FAULT;
    if(offset==0) {
        if(write) {
            if(*value>255) return DEVICE_FAULT;
            a->selector=(uint8_t)*value;
        } else *value=a->selector;
        return DEVICE_HANDLED;
    }
    if(offset!=0x10) return DEVICE_UNSUPPORTED;
    unsigned reg=a->selector;
    if(reg<3) {
        if(write) return DEVICE_UNSUPPORTED; /* fixed ID and arbitration ID */
        *value=reg==1?0x00170011:0;
        return DEVICE_HANDLED;
    }
    if(reg<0x10 || reg>=0x10+2*IOAPIC_PINS) return DEVICE_UNSUPPORTED;
    unsigned pin=(reg-0x10)/2;
    struct ioapic_pin *p=&a->pins[pin];
    if(!write) {
        *value=(reg&1)?0:p->low|(p->remote_irr?REMOTE:0);
        return DEVICE_HANDLED;
    }
    if(p->remote_irr) {
        /* A guest may mask an in-service level line before acknowledging
         * it; keep the delivery identity until EOI. Read-only Remote IRR
         * may be present in the normal read/modify/write value. */
        if(!a->guest_profile || (reg&1)) return DEVICE_FAULT;
        uint32_t low=*value&~REMOTE;
        if((low&~MASK)!=(p->low&~MASK)) return DEVICE_FAULT;
        if(!(low&MASK)) for(unsigned i=0;i<IOAPIC_PINS;i++)
            if(i!=pin && !(a->pins[i].low&MASK) &&
               (uint8_t)a->pins[i].low==(uint8_t)low) return DEVICE_FAULT;
        p->low=low;
        return DEVICE_HANDLED;
    }
    if(reg&1) return *value==0?DEVICE_HANDLED:DEVICE_FAULT;
    uint32_t low=*value;
    if((low&~(0xffu|MASK|LEVEL|(a->guest_profile?LOW_ACTIVE:0))) || (low&0xff)<32) return DEVICE_FAULT;
    if(!(low&MASK)) for(unsigned i=0;i<IOAPIC_PINS;i++)
        if(i!=pin && !(a->pins[i].low&MASK) &&
           (uint8_t)a->pins[i].low==(uint8_t)low) return DEVICE_FAULT;
    /* Register value is committed before delivery. A failed callback leaves
     * the valid route/line available for explicit retry, remote IRR clear. */
    p->low=low;
    return ioapic_retry(a,pin)?DEVICE_HANDLED:DEVICE_FAULT;
}
