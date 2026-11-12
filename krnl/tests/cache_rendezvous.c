#include "../cache_rendezvous.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define REJECT(call) do {struct cache_rendezvous old=r;assert(!(call));assert(!memcmp(&old,&r,sizeof r));}while(0)
static void cover(struct cache_rendezvous *r,uint64_t epoch)
{
    enum cache_rendezvous_phase phase=r->phase;
    assert(!cache_rendezvous_entry_allowed(r,epoch));
    assert(cache_rendezvous_ack(r,63,epoch,phase));
    assert(!cache_rendezvous_advance(r,epoch));
    assert(!cache_rendezvous_entry_allowed(r,epoch));
    assert(cache_rendezvous_ack(r,0,epoch,phase));
    assert(r->phase==phase); /* Final ACK does not advance. */
    assert(!cache_rendezvous_entry_allowed(r,epoch));
    assert(cache_rendezvous_advance(r,epoch));
}
int main(void)
{
    struct cache_rendezvous r={0};uint64_t epoch=123;
    REJECT(cache_rendezvous_init(&r,0));assert(!cache_rendezvous_init(0,1));
    assert(cache_rendezvous_init(&r,1ull|(1ull<<63)));
    assert(cache_rendezvous_entry_allowed(&r,1));
    REJECT(cache_rendezvous_begin(&r,1,true,false,&epoch));assert(epoch==123);
    REJECT(cache_rendezvous_begin(&r,1,false,true,&epoch));assert(epoch==123);
    REJECT(cache_rendezvous_begin(&r,0,true,true,&epoch));
    REJECT(cache_rendezvous_begin(&r,1,true,true,0));
    REJECT(cache_rendezvous_ack(&r,0,1,CACHE_RENDEZVOUS_OPEN));
    REJECT(cache_rendezvous_advance(&r,1));REJECT(cache_rendezvous_finish(&r,1));
    for(unsigned round=0;round<3;round++){
        uint64_t old=r.generation;
        assert(cache_rendezvous_begin(&r,old,true,true,&epoch));assert(epoch==old+1);
        assert(!cache_rendezvous_entry_allowed(&r,old));
        REJECT(cache_rendezvous_begin(&r,epoch,true,true,&epoch));
        for(unsigned i=0;i<3;i++){
            enum cache_rendezvous_phase phase=r.phase;
            REJECT(cache_rendezvous_ack(&r,0,epoch-1,phase));
            REJECT(cache_rendezvous_ack(&r,0,epoch+1,phase));
            REJECT(cache_rendezvous_ack(&r,0,epoch,(enum cache_rendezvous_phase)(phase+1)));
            REJECT(cache_rendezvous_ack(&r,1,epoch,phase));
            REJECT(cache_rendezvous_ack(&r,64,epoch,phase));
            REJECT(cache_rendezvous_advance(&r,epoch));REJECT(cache_rendezvous_finish(&r,epoch));
            assert(cache_rendezvous_ack(&r,0,epoch,phase));
            REJECT(cache_rendezvous_ack(&r,0,epoch,phase));
            REJECT(cache_rendezvous_advance(&r,epoch));
            assert(cache_rendezvous_ack(&r,63,epoch,phase));
            assert(r.phase==phase&&!cache_rendezvous_entry_allowed(&r,epoch));
            REJECT(cache_rendezvous_finish(&r,epoch));
            REJECT(cache_rendezvous_advance(&r,epoch-1));
            assert(cache_rendezvous_advance(&r,epoch));
        }
        assert(r.phase==CACHE_RENDEZVOUS_COMPLETE&&!cache_rendezvous_entry_allowed(&r,epoch));
        REJECT(cache_rendezvous_ack(&r,0,epoch,CACHE_RENDEZVOUS_COMPLETE));
        REJECT(cache_rendezvous_begin(&r,epoch,true,true,&epoch));
        REJECT(cache_rendezvous_advance(&r,epoch));REJECT(cache_rendezvous_finish(&r,epoch-1));
        assert(cache_rendezvous_finish(&r,epoch));assert(cache_rendezvous_entry_allowed(&r,epoch));
        assert(!cache_rendezvous_entry_allowed(&r,epoch-1));
    }
    r.generation=UINT64_MAX;epoch=77;
    REJECT(cache_rendezvous_begin(&r,UINT64_MAX,true,true,&epoch));assert(epoch==77);
    /* Timeouts at every pending phase, including completion before finish. */
    for(unsigned stage=0;stage<4;stage++){
        assert(cache_rendezvous_init(&r,1ull|(1ull<<63)));
        assert(cache_rendezvous_begin(&r,1,true,true,&epoch));
        for(unsigned n=0;n<stage;n++)cover(&r,epoch);
        uint64_t mask=r.acknowledged;
        cache_rendezvous_fail(&r);
        assert(r.phase==CACHE_RENDEZVOUS_POISON&&r.generation==epoch&&r.acknowledged==mask);
        assert(!cache_rendezvous_entry_allowed(&r,epoch));
        REJECT(cache_rendezvous_begin(&r,epoch,true,true,&epoch));
        REJECT(cache_rendezvous_ack(&r,0,epoch,CACHE_RENDEZVOUS_PRECHECK));
        REJECT(cache_rendezvous_advance(&r,epoch));REJECT(cache_rendezvous_finish(&r,epoch));
    }
    assert(cache_rendezvous_init(&r,1));cache_rendezvous_fail(&r);
    assert(!cache_rendezvous_entry_allowed(&r,1));
    assert(!cache_rendezvous_begin(0,1,true,true,&epoch));assert(!cache_rendezvous_ack(0,0,1,CACHE_RENDEZVOUS_TLB));
    assert(!cache_rendezvous_advance(0,1));assert(!cache_rendezvous_finish(0,1));
    assert(!cache_rendezvous_entry_allowed(0,1));cache_rendezvous_fail(0);
    puts("cache_rendezvous: PASS");
}
