#include "guest_apic.h"
#define MASK (1u<<16)
static int slot(uint32_t msr) {
    switch(msr) {
    case 0x82f:return 0;case 0x833:return 1;case 0x834:return 2;
    case 0x835:return 3;case 0x836:return 4;case 0x837:return 5;
    default:return -1;
    }
}
void guest_apic_init_mode(struct guest_apic *a,uint64_t now,bool x2apic) {
    *a=(struct guest_apic){.x2apic=x2apic,.dfr=UINT32_MAX};
    lapic_init(&a->irq);
    /* Priority core uses MSR numbering in either exposed register mode. */
    (void)lapic_write(&a->irq,0x1b,UINT64_C(0xfee00d00));
    lapic_timer_init_guest(&a->timer,now);
    for(unsigned i=0;i<6;i++) a->lvt[i]=MASK;
}
void guest_apic_init(struct guest_apic *a,uint64_t now) {
    guest_apic_init_mode(a,now,true);
}
bool guest_apic_enqueue(struct guest_apic *a,unsigned v,bool level) {
    if(!a || v<32 || v>255) return false;
    uint32_t bit=UINT32_C(1)<<(v%32);
    if((a->irq.irr[v/32]&bit) && !!(a->pending_level[v/32]&bit)!=level)
        return false;
    if((a->irq.isr[v/32]&bit) && !!(a->tmr[v/32]&bit)!=level)
        return false;
    if(!lapic_enqueue_external(&a->irq,v)) return false;
    if(level) a->tmr[v/32]|=bit;
    else a->tmr[v/32]&=~bit;
    if(level) a->pending_level[v/32]|=bit;
    else a->pending_level[v/32]&=~bit;
    return true;
}
bool guest_apic_select(const struct guest_apic *a,uint8_t *v) {
    return a && v && lapic_select(&a->irq,v);
}
bool guest_apic_accept(struct guest_apic *a,uint8_t v) {
    if(!a || !lapic_accept(&a->irq,v)) return false;
    uint32_t bit=UINT32_C(1)<<(v%32);
    a->tmr[v/32]=(a->tmr[v/32]&~bit)|(a->pending_level[v/32]&bit);
    a->pending_level[v/32]&=~bit;
    return true;
}
bool guest_apic_poll(struct guest_apic *a,uint64_t now) {
    if(!a) return false;
    struct guest_apic n=*a;uint8_t v;
    enum lapic_timer_event e=lapic_timer_poll(&n.timer,now,&v);
    if(e==LAPIC_TIMER_INVALID) return false;
    if(e==LAPIC_TIMER_IRQ && (n.irq.svr&0x100) && !guest_apic_enqueue(&n,v,false))
        return false;
    *a=n;return true;
}
static bool read_register(struct guest_apic *a,uint32_t msr,uint64_t now,uint64_t *out) {
    if(!a || !out || now<a->timer.last_now) return false;
    struct guest_apic n=*a;uint64_t v;
    int index=slot(msr);
    if(index>=0) v=a->lvt[index];
    else if(msr>=0x818 && msr<=0x81f) v=a->tmr[msr-0x818];
    else switch(msr) {
    case 0x1b:v=a->x2apic?UINT64_C(0xfee00d00):UINT64_C(0xfee00900);break;
    case 0x803:v=0x60014;break; /* max LVT index 6, no EOI suppression */
    case 0x80d:v=1;break; /* x2APIC ID0 cluster0 logical bit0 */
    case 0x828:v=0;break; /* unsupported messages reject, ESR stays clear */
    case 0x830:v=a->icr;break;
    case 0x832:case 0x838:case 0x839:case 0x83e:
        if(!lapic_timer_read(&n.timer,msr,now,&v)) return false;
        break;
    default:if(!lapic_read(&n.irq,msr,&v)) return false;
        break;
    }
    n.timer.last_now=now;*a=n;*out=v;return true;
}
static bool write_inner(struct guest_apic *a,uint32_t msr,uint64_t value,
                        uint64_t now,struct guest_apic_effect *effect) {
    int index=slot(msr);
    if(index>=0) {
        /* Unsupported device sources stay masked. Preserve programmable
         * vector/delivery/polarity/trigger fields, reject status/reserved. */
        uint64_t allowed=0x100ff;
        if(msr==0x835 || msr==0x836) allowed|=0xa700;
        else if(msr!=0x837) allowed|=0x700;
        if(!(value&MASK) || (value&~allowed)) return false;
        a->lvt[index]=(uint32_t)value;return true;
    }
    switch(msr) {
    case 0x1b:
        if(value==(a->x2apic?UINT64_C(0xfee00d00):UINT64_C(0xfee00900))) return true;
        if(!a->x2apic && value==UINT64_C(0xfee00d00)) {a->x2apic=true;return true;}
        return false; /* disable/relocation/back-transition not supported */
    case 0x80b:
        if(value) return false;
        if(lapic_eoi_vector(&a->irq,&effect->vector)) {
            unsigned v=effect->vector;
            effect->eoi=true;
            effect->level=(a->tmr[v/32]&(UINT32_C(1)<<(v%32)))!=0;
            if(!(a->irq.irr[v/32]&(UINT32_C(1)<<(v%32))))
                a->tmr[v/32]&=~(UINT32_C(1)<<(v%32));
        }
        return true;
    case 0x828:return value==0;
    case 0x830: {
        /* Fixed edge/asserted self IPI: explicit physical ID0, self shorthand
         * or all-including-self with only BSP present. No lowest/NMI/INIT/SIPI. */
        uint32_t lo=(uint32_t)value;
        unsigned shorthand=(lo>>18)&3;
        if((value>>32)!=0 || (lo&~0xc40ffu) || (lo&0xff)<32 || shorthand==3)
            return false;
        if(!guest_apic_enqueue(a,lo&0xff,false)) return false;
        a->icr=value;return true;
    }
    case 0x83f:
        if(value>255) return false;
        return guest_apic_enqueue(a,(unsigned)value,false);
    case 0x832:case 0x838:case 0x83e:
        return lapic_timer_write(&a->timer,msr,value,now);
    default:return lapic_write(&a->irq,msr,value);
    }
}
static bool write_register(struct guest_apic *a,uint32_t msr,uint64_t value,
                       uint64_t now,struct guest_apic_effect *effect) {
    if(!a || !effect || now<a->timer.last_now) return false;
    struct guest_apic n=*a;
    struct guest_apic_effect result={0};
    if(!write_inner(&n,msr,value,now,&result)) return false;
    n.timer.last_now=now;*a=n;*effect=result;return true;
}

bool guest_apic_read(struct guest_apic *a,uint32_t msr,uint64_t now,uint64_t *out) {
    if(!a || (!a->x2apic && msr!=0x1b)) return false;
    return read_register(a,msr,now,out);
}
bool guest_apic_write(struct guest_apic *a,uint32_t msr,uint64_t value,
                       uint64_t now,struct guest_apic_effect *effect) {
    if(!a || (!a->x2apic && msr!=0x1b)) return false;
    return write_register(a,msr,value,now,effect);
}
bool guest_apic_mmio(struct guest_apic *a,uint64_t offset,unsigned width,
                    bool write,uint32_t *value,uint64_t now,
                    struct guest_apic_effect *effect) {
    if(!a || !value || !effect || a->x2apic || width!=4 || (offset&15) ||
       offset>=0x400 || offset==0x3f0 || now<a->timer.last_now) return false;
    struct guest_apic n=*a;
    struct guest_apic_effect e={0};uint64_t out=*value;
    if(offset==0xd0 || offset==0xe0) {
        if(write) {
            if(offset==0xd0) {
                if(*value&0xffffff) return false;
                n.ldr=*value;
            } else {if(*value!=UINT32_MAX) return false;n.dfr=*value;}
        } else out=offset==0xd0?n.ldr:n.dfr;
    } else if(offset==0x310) {
        if(write && *value!=0) return false; /* physical destination ID0 only */
        out=0;
    } else if(write) {
        if(!write_register(&n,0x800+(uint32_t)(offset/16),*value,now,&e)) return false;
    } else {
        if(!read_register(&n,0x800+(uint32_t)(offset/16),now,&out)) return false;
    }
    n.timer.last_now=now;*a=n;*effect=e;
    if(!write) *value=(uint32_t)out;
    return true;
}
