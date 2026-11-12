#include "../cache_mp_transition.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define REJECT(call) do {struct cache_mp_transition before=t;assert(!(call));assert(!memcmp(&t,&before,sizeof t));}while(0)
static bool publication(enum cache_mp_phase p)
{return p==CACHE_MP_DETACH_EPT||p==CACHE_MP_BREAK_TLB||p==CACHE_MP_MAKE_TLB||p==CACHE_MP_PUBLISH_EPT;}
static void complete_phase(struct cache_mp_transition *t,uint64_t epoch)
{
    enum cache_mp_phase p=t->phase;
    if(publication(p))assert(cache_mp_transition_publish(t,epoch,p));
    assert(cache_mp_transition_ack(t,0,epoch,p));
    assert(cache_mp_transition_ack(t,63,epoch,p));
    assert(cache_mp_transition_advance(t,epoch));
}
int main(void)
{
    struct cache_mp_transition t={0};uint64_t epoch=123;
    REJECT(cache_mp_transition_init(&t,0));assert(!cache_mp_transition_init(0,1));
    assert(cache_mp_transition_init(&t,1ull|(1ull<<63)));
    assert(cache_mp_transition_entry_allowed(&t,1));
    REJECT(cache_mp_transition_begin(&t,1,CACHE_MP_OPEN_UC,true,false,&epoch));assert(epoch==123);
    REJECT(cache_mp_transition_begin(&t,1,CACHE_MP_OPEN_UC,false,true,&epoch));assert(epoch==123);
    REJECT(cache_mp_transition_begin(&t,0,CACHE_MP_OPEN_UC,true,true,&epoch));
    REJECT(cache_mp_transition_begin(&t,1,CACHE_MP_OPEN_WB,true,true,&epoch));
    REJECT(cache_mp_transition_begin(&t,1,CACHE_MP_COMPLETE,true,true,&epoch));
    REJECT(cache_mp_transition_begin(&t,1,CACHE_MP_OPEN_UC,true,true,0));
    REJECT(cache_mp_transition_ack(&t,0,1,CACHE_MP_OPEN_WB));
    for(unsigned round=0;round<4;round++){
        enum cache_mp_phase dest=t.phase==CACHE_MP_OPEN_WB?CACHE_MP_OPEN_UC:CACHE_MP_OPEN_WB;
        uint64_t previous=t.generation;
        assert(cache_mp_transition_begin(&t,previous,dest,true,true,&epoch));assert(epoch==previous+1);
        REJECT(cache_mp_transition_begin(&t,epoch,dest,true,true,&epoch));
        for(enum cache_mp_phase p=CACHE_MP_DETACH_EPT;p<=CACHE_MP_PUBLISH_EPT;p++){
            assert(t.phase==p&&!cache_mp_transition_entry_allowed(&t,epoch));
            REJECT(cache_mp_transition_advance(&t,epoch));REJECT(cache_mp_transition_finish(&t,epoch));
            REJECT(cache_mp_transition_publish(&t,epoch-1,p));
            REJECT(cache_mp_transition_publish(&t,epoch,(enum cache_mp_phase)(p+1)));
            if(publication(p)){
                REJECT(cache_mp_transition_ack(&t,0,epoch,p));
                assert(cache_mp_transition_publish(&t,epoch,p));
            }else REJECT(cache_mp_transition_publish(&t,epoch,p));
            REJECT(cache_mp_transition_publish(&t,epoch,p));
            REJECT(cache_mp_transition_ack(&t,0,epoch-1,p));
            REJECT(cache_mp_transition_ack(&t,0,epoch+1,p));
            REJECT(cache_mp_transition_ack(&t,0,epoch,(enum cache_mp_phase)(p+1)));
            REJECT(cache_mp_transition_ack(&t,1,epoch,p));REJECT(cache_mp_transition_ack(&t,64,epoch,p));
            assert(cache_mp_transition_ack(&t,63,epoch,p));
            REJECT(cache_mp_transition_ack(&t,63,epoch,p));REJECT(cache_mp_transition_advance(&t,epoch));
            assert(cache_mp_transition_ack(&t,0,epoch,p));
            assert(t.phase==p&&!cache_mp_transition_entry_allowed(&t,epoch));
            REJECT(cache_mp_transition_finish(&t,epoch));REJECT(cache_mp_transition_advance(&t,epoch-1));
            assert(cache_mp_transition_advance(&t,epoch));
        }
        assert(t.phase==CACHE_MP_COMPLETE&&!cache_mp_transition_entry_allowed(&t,epoch));
        REJECT(cache_mp_transition_advance(&t,epoch));REJECT(cache_mp_transition_finish(&t,epoch-1));
        REJECT(cache_mp_transition_begin(&t,epoch,dest,true,true,&epoch));
        assert(cache_mp_transition_finish(&t,epoch));assert(t.phase==dest&&cache_mp_transition_entry_allowed(&t,epoch));
        assert(!cache_mp_transition_entry_allowed(&t,previous));
    }
    assert(t.phase==CACHE_MP_OPEN_WB&&t.generation==5);
    t.generation=UINT64_MAX;epoch=123;
    REJECT(cache_mp_transition_begin(&t,UINT64_MAX,CACHE_MP_OPEN_UC,true,true,&epoch));assert(epoch==123);
    /* Poison at all six phases and COMPLETE, retaining snapshot counters. */
    for(unsigned stage=0;stage<7;stage++){
        assert(cache_mp_transition_init(&t,1ull|(1ull<<63)));
        assert(cache_mp_transition_begin(&t,1,CACHE_MP_OPEN_UC,true,true,&epoch));
        for(unsigned n=0;n<stage;n++)complete_phase(&t,epoch);
        struct cache_mp_transition before=t;
        cache_mp_transition_fail(&t);
        assert(t.phase==CACHE_MP_POISON&&t.generation==before.generation&&t.acknowledged==before.acknowledged&&t.published==before.published);
        assert(!cache_mp_transition_entry_allowed(&t,epoch));
        REJECT(cache_mp_transition_begin(&t,epoch,CACHE_MP_OPEN_WB,true,true,&epoch));
        REJECT(cache_mp_transition_publish(&t,epoch,CACHE_MP_DETACH_EPT));
        REJECT(cache_mp_transition_ack(&t,0,epoch,CACHE_MP_DETACH_EPT));
        REJECT(cache_mp_transition_advance(&t,epoch));REJECT(cache_mp_transition_finish(&t,epoch));
    }
    assert(!cache_mp_transition_begin(0,1,CACHE_MP_OPEN_UC,true,true,&epoch));
    assert(!cache_mp_transition_publish(0,1,CACHE_MP_DETACH_EPT));assert(!cache_mp_transition_ack(0,0,1,CACHE_MP_DETACH_EPT));
    assert(!cache_mp_transition_advance(0,1));assert(!cache_mp_transition_finish(0,1));
    assert(!cache_mp_transition_entry_allowed(0,1));cache_mp_transition_fail(0);
    puts("cache_mp_transition: PASS");
}
