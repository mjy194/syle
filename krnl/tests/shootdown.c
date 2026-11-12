#include "../shootdown.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define REJECT(call) do { struct shootdown old=s; assert(!(call)); assert(memcmp(&s,&old,sizeof(s))==0); } while(0)
int main(void)
{
    struct shootdown s={0};uint64_t next=123;
    REJECT(shootdown_init(&s,0));assert(!shootdown_init(0,1));
    assert(shootdown_init(&s,1ull|(1ull<<63)));
    assert(s.generation==1 && s.phase==SHOOTDOWN_READY && !s.running && !s.acknowledged);
    REJECT(shootdown_enter(&s,64,1));REJECT(shootdown_enter(&s,1,1));
    REJECT(shootdown_enter(&s,0,0));REJECT(shootdown_enter(&s,0,2));
    REJECT(shootdown_leave(&s,0));REJECT(shootdown_ack(&s,0,1));
    REJECT(shootdown_published(&s));REJECT(shootdown_begin(&s,0,&next));assert(next==123);
    REJECT(shootdown_begin(&s,1,0));
    assert(shootdown_enter(&s,0,1));assert(shootdown_enter(&s,63,1));
    REJECT(shootdown_enter(&s,0,1));REJECT(shootdown_begin(&s,1,&next));assert(next==123);
    assert(shootdown_leave(&s,0));REJECT(shootdown_leave(&s,0));
    REJECT(shootdown_begin(&s,1,&next));assert(shootdown_leave(&s,63));
    for(unsigned round=0;round<3;round++){
        uint64_t previous=s.generation;
        assert(shootdown_begin(&s,previous,&next));assert(next==previous+1);
        assert(s.phase==SHOOTDOWN_PUBLISHING && s.acknowledged==0);
        REJECT(shootdown_begin(&s,next,&next));
        REJECT(shootdown_ack(&s,0,s.generation));
        REJECT(shootdown_enter(&s,0,s.generation));REJECT(shootdown_leave(&s,0));
        assert(shootdown_published(&s));REJECT(shootdown_published(&s));
        REJECT(shootdown_ack(&s,0,previous));REJECT(shootdown_ack(&s,0,next+1));
        REJECT(shootdown_ack(&s,1,next));REJECT(shootdown_ack(&s,64,next));
        assert(shootdown_ack(&s,63,next));assert(s.phase==SHOOTDOWN_WAIT);
        REJECT(shootdown_ack(&s,63,next));REJECT(shootdown_enter(&s,63,next));
        assert(shootdown_ack(&s,0,next));assert(s.phase==SHOOTDOWN_READY);
        REJECT(shootdown_ack(&s,0,next));
        assert(shootdown_enter(&s,63,next));assert(shootdown_leave(&s,63));
    }
    assert(s.generation==4);
    s.generation=UINT64_MAX;next=456;
    REJECT(shootdown_begin(&s,UINT64_MAX,&next));assert(next==456);
    for(unsigned phase=0;phase<3;phase++){
        assert(shootdown_init(&s,3));
        if(phase){assert(shootdown_begin(&s,1,&next));if(phase==2)assert(shootdown_published(&s));}
        struct shootdown old=s;
        shootdown_fail(&s);assert(s.phase==SHOOTDOWN_POISON);
        assert(s.generation==old.generation && s.running==old.running && s.acknowledged==old.acknowledged);
        REJECT(shootdown_enter(&s,0,s.generation));REJECT(shootdown_leave(&s,0));
        REJECT(shootdown_begin(&s,s.generation,&next));REJECT(shootdown_published(&s));
        REJECT(shootdown_ack(&s,0,s.generation));
        shootdown_fail(&s);assert(s.phase==SHOOTDOWN_POISON);
    }
    assert(shootdown_init(&s,1));assert(shootdown_enter(&s,0,1));shootdown_fail(&s);
    assert(s.running==1);REJECT(shootdown_leave(&s,0));
    assert(!shootdown_enter(0,0,1));assert(!shootdown_leave(0,0));assert(!shootdown_begin(0,1,&next));
    assert(!shootdown_published(0));assert(!shootdown_ack(0,0,1));shootdown_fail(0);
    assert(shootdown_init(&s,1));assert(shootdown_begin(&s,1,&next));
    assert(shootdown_published(&s));assert(shootdown_ack(&s,0,next));assert(s.phase==SHOOTDOWN_READY);
    puts("shootdown: PASS");
}
