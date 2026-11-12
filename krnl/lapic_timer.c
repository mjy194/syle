#include "lapic_timer.h"

static unsigned divisor(uint32_t config) {
    /* Encoding bits [3,1,0]; bit2 is reserved. */
    static const unsigned values[8] = {2,4,8,16,32,64,128,1};
    return values[(config & 3) | ((config >> 1) & 4)];
}
static bool periodic(const struct lapic_timer *t) {
    return t->periodic_supported && (t->lvt&(1u<<17));
}
/* Called only at/after deadline. Calculate without changing the timer so
 * overflow cannot partly consume pending expiry or advance last_now. */
static bool next_phase(const struct lapic_timer *t, uint64_t now,
                        uint64_t *next, uint64_t *expirations) {
    if(!t->period || now<t->deadline) return false;
    uint64_t elapsed=(now-t->deadline)/t->period;
    if(elapsed==UINT64_MAX) return false;
    uint64_t count=elapsed+1;
    if(count>(UINT64_MAX-t->deadline)/t->period) return false;
    *next=t->deadline+count*t->period;
    *expirations=count;
    return true;
}
void lapic_timer_init_mode(struct lapic_timer *t, uint64_t now,
                           bool periodic_supported) {
    *t=(struct lapic_timer){.lvt=1u<<16,.last_now=now,
                            .periodic_supported=periodic_supported};
}
void lapic_timer_init(struct lapic_timer *t, uint64_t now) {
    lapic_timer_init_mode(t,now,false);
}
void lapic_timer_init_guest(struct lapic_timer *t, uint64_t now) {
    lapic_timer_init_mode(t,now,true);
    t->guest_profile=true;
}
bool lapic_timer_mmio(struct lapic_timer *t,uint64_t offset,unsigned width,
                      bool write,uint32_t *value,uint64_t now) {
    if(!t || !value || width!=4) return false;
    uint32_t msr;
    switch(offset) {
    case 0x320: msr=0x832;break;
    case 0x380: msr=0x838;break;
    case 0x390: msr=0x839;break;
    case 0x3e0: msr=0x83e;break;
    default: return false;
    }
    if(write) return lapic_timer_write(t,msr,*value,now);
    uint64_t result;
    if(!lapic_timer_read(t,msr,now,&result)) return false;
    *value=(uint32_t)result;
    return true;
}
bool lapic_timer_read(struct lapic_timer *t,uint32_t msr,
                      uint64_t now,uint64_t *value) {
    if(now<t->last_now) return false;
    uint64_t result;
    switch(msr) {
    case 0x832: result=t->lvt;break;
    case 0x838: result=t->initial;break;
    case 0x83e: result=t->divide;break;
    case 0x839:
        result=0;
        if(t->armed) {
            uint64_t deadline=t->deadline;
            if(now>=deadline && periodic(t)) {
                uint64_t ignored;
                if(!next_phase(t,now,&deadline,&ignored)) return false;
            }
            if(now<deadline) {
                uint64_t remaining=deadline-now;
                uint64_t quantum=LAPIC_TIMER_BASE_TICKS*divisor(t->divide);
                result=remaining/quantum+(remaining%quantum!=0);
            }
        }
        break;
    default: return false;
    }
    t->last_now=now;*value=result;return true;
}
bool lapic_timer_write(struct lapic_timer *t,uint32_t msr,
                       uint64_t value,uint64_t now) {
    if(now<t->last_now) return false;
    switch(msr) {
    case 0x832:
        /* Fixed delivery; optional periodic only. Deadline mode and
         * reserved/status fields remain rejected in both profiles. */
        if((t->armed && (!t->guest_profile || now>=t->deadline)) ||
           (value&~UINT64_C(0x300ff)) ||
           ((value&0xff)<32 && !(t->guest_profile && (value&(1u<<16)))) ||
           ((value&(1u<<17)) && !t->periodic_supported))
            return false;
        t->lvt=(uint32_t)value;
        break;
    case 0x83e:
        if(t->armed || (value&~UINT64_C(0xb))) return false;
        t->divide=(uint32_t)value;
        break;
    case 0x838: {
        if(value>UINT32_MAX) return false;
        uint64_t delta=value*LAPIC_TIMER_BASE_TICKS*divisor(t->divide);
        if(value && (((t->lvt&0xff)<32 && !(t->guest_profile && (t->lvt&(1u<<16)))) || delta>UINT64_MAX-now)) return false;
        t->initial=(uint32_t)value;
        t->armed=value!=0;
        t->deadline=value?now+delta:0;
        t->period=delta;
        t->last_expirations=0;
        break;
    }
    default: return false; /* includes read-only current-count register */
    }
    t->last_now=now;
    return true;
}
enum lapic_timer_event lapic_timer_poll(struct lapic_timer *t,
                                       uint64_t now,uint8_t *vector) {
    if(now<t->last_now) return LAPIC_TIMER_INVALID;
    if(!t->armed || now<t->deadline) {
        t->last_now=now;t->last_expirations=0;
        return LAPIC_TIMER_NONE;
    }
    uint64_t next=0,count=1;
    if(periodic(t) && !next_phase(t,now,&next,&count)) return LAPIC_TIMER_INVALID;
    t->last_now=now;t->last_expirations=count;
    if(periodic(t)) t->deadline=next;
    else t->armed=false; /* consume one-shot expiry even if masked */
    if(t->lvt&(1u<<16)) return LAPIC_TIMER_MASKED;
    *vector=(uint8_t)t->lvt;
    return LAPIC_TIMER_IRQ;
}
bool lapic_timer_reload(const struct lapic_timer *t,uint64_t now,
                         unsigned shift,uint32_t *ticks) {
    if(now<t->last_now || shift>31) return false;
    uint64_t result=0;
    if(t->armed && now<t->deadline) {
        uint64_t delta=t->deadline-now;
        uint64_t quantum=UINT64_C(1)<<shift;
        result=delta/quantum+(delta%quantum!=0);
        if(result>UINT32_MAX) result=UINT32_MAX;
    }
    *ticks=(uint32_t)result;return true;
}
bool lapic_timer_caps(uint64_t pin,uint64_t exit) {
    const uint64_t p=UINT64_C(1)<<6,e=UINT64_C(1)<<22;
    return !(pin&p) && ((pin>>32)&p) && !(exit&e) && ((exit>>32)&e);
}
