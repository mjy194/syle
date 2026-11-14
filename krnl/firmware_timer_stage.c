#include "firmware_timer_stage.h"
bool fts_init(struct fts_state *s,uint64_t domain,uint64_t now) {
    if(!s || !domain) return false;
    lapic_timer_init_guest(&s->timer,now);s->domain=domain;return true;
}
static enum fts_result advance(struct fts_state *s,uint64_t domain,uint64_t now) {
    if(!s || !domain || s->domain!=domain || now<s->timer.last_now) return FTS_INVALID;
    /* Never call the consuming base-model poll for an unowned IRQ. This also
     * blocks a same-time mask/zero-count write from erasing the event. */
    if(s->timer.armed && now>=s->timer.deadline && !(s->timer.lvt&(1u<<16)))
        return FTS_STOP_IRQ;
    uint8_t ignored=0;
    enum lapic_timer_event e=lapic_timer_poll(&s->timer,now,&ignored);
    return e==LAPIC_TIMER_INVALID || e==LAPIC_TIMER_IRQ ? FTS_INVALID : FTS_OK;
}
enum fts_result fts_poll(struct fts_state *s,uint64_t domain,uint64_t now) {
    if(!s) return FTS_INVALID;
    struct fts_state next=*s;
    enum fts_result r=advance(&next,domain,now);
    if(r==FTS_OK) *s=next;
    return r;
}
enum fts_result fts_mmio(struct fts_state *s,uint64_t domain,uint64_t now,
                        uint64_t off,unsigned width,bool write,uint32_t *value) {
    if(!s || !value || width!=4) return FTS_INVALID;
    struct fts_state next=*s;uint32_t out=*value;
    enum fts_result r=advance(&next,domain,now);
    if(r!=FTS_OK) return r;
    if(write && off==0x320 && !next.timer.armed) {
        /* Register setup is not an interrupt. Permit low vector while idle,
         * but never start it or admit it on a running unmasked timer. */
        if(out&~UINT32_C(0x300ff)) return FTS_INVALID;
        next.timer.lvt=out;
    } else {
        bool unmasked=!(next.timer.lvt&(1u<<16));
        if(write && off==0x380 && out && unmasked && (next.timer.lvt&255)<32)
            return FTS_STOP_VECTOR;
        if(write && off==0x320 && next.timer.armed && !(out&(1u<<16)) && (out&255)<32)
            return FTS_STOP_VECTOR;
        if(!lapic_timer_mmio(&next.timer,off,width,write,&out,now)) return FTS_INVALID;
    }
    *s=next;*value=out;return FTS_OK;
}
