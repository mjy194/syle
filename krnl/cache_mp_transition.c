#include "cache_mp_transition.h"
static bool valid(const struct cache_mp_transition *t)
{
    return t && t->participants && t->generation && !(t->acknowledged&~t->participants);
}
static bool work(enum cache_mp_phase p)
{return p>=CACHE_MP_DETACH_EPT && p<=CACHE_MP_PUBLISH_EPT;}
static bool needs_publication(enum cache_mp_phase p)
{return p==CACHE_MP_DETACH_EPT || p==CACHE_MP_BREAK_TLB || p==CACHE_MP_MAKE_TLB || p==CACHE_MP_PUBLISH_EPT;}
static bool open(enum cache_mp_phase p)
{return p==CACHE_MP_OPEN_WB || p==CACHE_MP_OPEN_UC;}
bool cache_mp_transition_init(struct cache_mp_transition *t,uint64_t mask)
{
    if(!t || !mask)return false;
    *t=(struct cache_mp_transition){.participants=mask,.generation=1,
        .phase=CACHE_MP_OPEN_WB,.destination=CACHE_MP_OPEN_WB};return true;
}
bool cache_mp_transition_begin(struct cache_mp_transition *t,uint64_t expected,
    enum cache_mp_phase destination,bool quiesced,bool resources,uint64_t *epoch)
{
    if(!valid(t) || !epoch || !quiesced || !resources || !open(t->phase) ||
       !open(destination) || destination==t->phase || expected!=t->generation ||
       t->generation==UINT64_MAX)return false;
    t->destination=destination;t->phase=CACHE_MP_DETACH_EPT;t->published=false;
    t->acknowledged=0;*epoch=++t->generation;return true;
}
bool cache_mp_transition_publish(struct cache_mp_transition *t,uint64_t epoch,enum cache_mp_phase phase)
{
    if(!valid(t) || epoch!=t->generation || phase!=t->phase || !needs_publication(phase) ||
       t->published || t->acknowledged)return false;
    t->published=true;return true;
}
bool cache_mp_transition_ack(struct cache_mp_transition *t,unsigned cpu,uint64_t epoch,enum cache_mp_phase phase)
{
    if(!valid(t) || epoch!=t->generation || phase!=t->phase || !work(phase) || cpu>=64 ||
       (needs_publication(phase)&&!t->published))return false;
    uint64_t bit=UINT64_C(1)<<cpu;
    if(!(t->participants&bit) || (t->acknowledged&bit))return false;
    t->acknowledged|=bit;return true;
}
bool cache_mp_transition_advance(struct cache_mp_transition *t,uint64_t epoch)
{
    if(!valid(t) || epoch!=t->generation || !work(t->phase) ||
       t->acknowledged!=t->participants || (needs_publication(t->phase)&&!t->published))return false;
    t->phase=(enum cache_mp_phase)(t->phase+1);
    if(t->phase!=CACHE_MP_COMPLETE){t->acknowledged=0;t->published=false;}
    return true;
}
bool cache_mp_transition_finish(struct cache_mp_transition *t,uint64_t epoch)
{
    if(!valid(t) || epoch!=t->generation || t->phase!=CACHE_MP_COMPLETE ||
       t->acknowledged!=t->participants || !t->published || !open(t->destination))return false;
    t->phase=t->destination;return true;
}
bool cache_mp_transition_entry_allowed(const struct cache_mp_transition *t,uint64_t epoch)
{return valid(t) && epoch==t->generation && open(t->phase);}
void cache_mp_transition_fail(struct cache_mp_transition *t)
{if(t)t->phase=CACHE_MP_POISON;}
