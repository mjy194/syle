#include "firmware_timer_session.h"
bool ftss_init(struct fts_session *s,uint64_t d,uint64_t now) {
    if(!s || s->initialized || s->terminal || !d) return false;
    if(!fts_init(&s->device,d,now)) return false;
    s->observed_now=now;s->initialized=true;s->reason=FTS_OK;return true;
}
static enum fts_result admit(struct fts_session *s,uint64_t d,uint64_t now) {
    if(!s || !s->initialized) return FTS_INVALID;
    if(s->terminal) return s->reason;
    if(d!=s->device.domain || now<s->observed_now) {
        s->terminal=true;s->reason=FTS_INVALID;return FTS_INVALID;
    }
    /* Track accepted clock observations even if the operation later rejects. */
    s->observed_now=now;
    if(s->device.timer.armed && now>=s->device.timer.deadline &&
       !(s->device.timer.lvt&(1u<<16))) {
        s->terminal=true;s->reason=FTS_STOP_IRQ;return FTS_STOP_IRQ;
    }
    return FTS_OK;
}
static enum fts_result finish(struct fts_session *s,enum fts_result r) {
    if(r==FTS_STOP_IRQ || r==FTS_STOP_VECTOR) { s->terminal=true;s->reason=r; }
    return r;
}
enum fts_result ftss_poll(struct fts_session *s,uint64_t d,uint64_t now) {
    enum fts_result r=admit(s,d,now);
    if(r!=FTS_OK) return r;
    return finish(s,fts_poll(&s->device,d,now));
}
enum fts_result ftss_mmio(struct fts_session *s,uint64_t d,uint64_t now,
                         uint64_t off,unsigned width,bool write,uint32_t *v) {
    enum fts_result r=admit(s,d,now);
    if(r!=FTS_OK) return r;
    return finish(s,fts_mmio(&s->device,d,now,off,width,write,v));
}
void ftss_abort(struct fts_session *s) {
    if(s && !s->terminal) {s->terminal=true;s->reason=FTS_INVALID;}
}
