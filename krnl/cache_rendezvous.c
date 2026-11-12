#include "cache_rendezvous.h"
static bool valid(const struct cache_rendezvous *r)
{
    return r && r->participants && r->generation &&
        !(r->acknowledged&~r->participants);
}
bool cache_rendezvous_init(struct cache_rendezvous *r,uint64_t participants)
{
    if(!r || !participants)return false;
    *r=(struct cache_rendezvous){.participants=participants,.generation=1,
        .phase=CACHE_RENDEZVOUS_OPEN};return true;
}
bool cache_rendezvous_begin(struct cache_rendezvous *r,uint64_t expected,
    bool quiesced,bool resources,uint64_t *epoch)
{
    if(!valid(r) || !epoch || !quiesced || !resources ||
       r->phase!=CACHE_RENDEZVOUS_OPEN || expected!=r->generation ||
       r->generation==UINT64_MAX)return false;
    ++r->generation;r->acknowledged=0;r->phase=CACHE_RENDEZVOUS_PRECHECK;
    *epoch=r->generation;return true;
}
bool cache_rendezvous_ack(struct cache_rendezvous *r,unsigned cpu,uint64_t epoch,
    enum cache_rendezvous_phase phase)
{
    if(!valid(r) || epoch!=r->generation || cpu>=64 || phase!=r->phase ||
       (phase!=CACHE_RENDEZVOUS_PRECHECK && phase!=CACHE_RENDEZVOUS_TLB &&
        phase!=CACHE_RENDEZVOUS_CACHE))return false;
    uint64_t bit=UINT64_C(1)<<cpu;
    if(!(r->participants&bit) || (r->acknowledged&bit))return false;
    r->acknowledged|=bit;return true;
}
bool cache_rendezvous_advance(struct cache_rendezvous *r,uint64_t epoch)
{
    if(!valid(r) || epoch!=r->generation || r->acknowledged!=r->participants)return false;
    switch(r->phase){
    case CACHE_RENDEZVOUS_PRECHECK:r->phase=CACHE_RENDEZVOUS_TLB;r->acknowledged=0;return true;
    case CACHE_RENDEZVOUS_TLB:r->phase=CACHE_RENDEZVOUS_CACHE;r->acknowledged=0;return true;
    case CACHE_RENDEZVOUS_CACHE:r->phase=CACHE_RENDEZVOUS_COMPLETE;return true;
    default:return false;
    }
}
bool cache_rendezvous_finish(struct cache_rendezvous *r,uint64_t epoch)
{
    if(!valid(r) || epoch!=r->generation || r->phase!=CACHE_RENDEZVOUS_COMPLETE ||
       r->acknowledged!=r->participants)return false;
    r->phase=CACHE_RENDEZVOUS_OPEN;return true;
}
bool cache_rendezvous_entry_allowed(const struct cache_rendezvous *r,uint64_t epoch)
{return valid(r) && epoch==r->generation && r->phase==CACHE_RENDEZVOUS_OPEN;}
void cache_rendezvous_fail(struct cache_rendezvous *r)
{if(r)r->phase=CACHE_RENDEZVOUS_POISON;}
