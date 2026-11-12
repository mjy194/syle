#include "../lapic_timer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t read_at(struct lapic_timer *t,uint32_t msr,uint64_t now) {
    uint64_t value=UINT64_MAX;
    assert(lapic_timer_read(t,msr,now,&value));
    return value;
}
static void reject_write(struct lapic_timer *t,uint32_t msr,uint64_t v,uint64_t now) {
    struct lapic_timer old;
    memcpy(&old,t,sizeof old);
    assert(!lapic_timer_write(t,msr,v,now));
    assert(memcmp(&old,t,sizeof old)==0);
}
static void periodic_tests(void) {
    struct lapic_timer t,old;
    uint8_t vector=123;
    uint32_t ticks=123;
    uint64_t value=123;
    lapic_timer_init_mode(&t,0,true);
    assert(t.periodic_supported);
    reject_write(&t,0x832,0x40040,0); /* deadline still unsupported */
    reject_write(&t,0x832,0x60040,0); /* reserved mode */
    assert(lapic_timer_write(&t,0x832,0x20040,0));
    assert(lapic_timer_write(&t,0x83e,11,0));
    assert(lapic_timer_write(&t,0x838,4,0));
    assert(t.period==4096 && t.deadline==4096 && t.last_expirations==0);
    assert(read_at(&t,0x839,4095)==1);
    assert(read_at(&t,0x839,4096)==4); /* conceptual reload at exact boundary */
    assert(t.deadline==4096 && t.armed); /* read did not consume event */
    assert(lapic_timer_reload(&t,4096,0,&ticks) && ticks==0); /* poll required */
    assert(lapic_timer_poll(&t,4096,&vector)==LAPIC_TIMER_IRQ && vector==0x40);
    assert(t.deadline==8192 && t.armed && t.last_expirations==1);
    reject_write(&t,0x80b,0,4096); /* EOI belongs to LAPIC, not timer reload */
    reject_write(&t,0x832,0x30040,4096); /* config remains locked while armed */
    assert(lapic_timer_poll(&t,9000,&vector)==LAPIC_TIMER_IRQ);
    assert(t.deadline==12288 && t.last_expirations==1); /* not now+period */
    assert(lapic_timer_poll(&t,30000,&vector)==LAPIC_TIMER_IRQ);
    assert(t.deadline==32768 && t.last_expirations==5); /* only one event */
    vector=123;
    assert(lapic_timer_poll(&t,30000,&vector)==LAPIC_TIMER_NONE && vector==123);
    assert(t.last_expirations==0);
    assert(read_at(&t,0x839,32768)==4);
    assert(t.deadline==32768);
    assert(lapic_timer_poll(&t,32768,&vector)==LAPIC_TIMER_IRQ);
    assert(t.deadline==36864 && t.last_expirations==1);

    lapic_timer_init_mode(&t,100,true);
    assert(lapic_timer_write(&t,0x832,0x30040,100));
    assert(lapic_timer_write(&t,0x83e,11,100));
    assert(lapic_timer_write(&t,0x838,2,100));
    vector=123;
    assert(lapic_timer_poll(&t,10000,&vector)==LAPIC_TIMER_MASKED && vector==123);
    assert(t.armed && t.last_expirations==4 && t.deadline==10340);
    reject_write(&t,0x832,0x20040,10000); /* must cancel to change mask */
    assert(lapic_timer_write(&t,0x838,0,10000));
    assert(!t.armed && t.period==0 && t.last_expirations==0);
    assert(lapic_timer_write(&t,0x832,0x20040,10000));
    assert(lapic_timer_poll(&t,20000,&vector)==LAPIC_TIMER_NONE && vector==123);
    assert(lapic_timer_write(&t,0x838,3,20000));
    assert(t.deadline==23072);
    assert(lapic_timer_write(&t,0x838,2,21000)); /* explicit rewrite resets phase */
    assert(t.deadline==23048 && t.period==2048);
    assert(lapic_timer_poll(&t,23048,&vector)==LAPIC_TIMER_IRQ);
    assert(t.deadline==25096 && t.last_expirations==1);
    memcpy(&old,&t,sizeof old);
    assert(lapic_timer_poll(&t,23047,&vector)==LAPIC_TIMER_INVALID);
    assert(memcmp(&old,&t,sizeof old)==0);

    /* Valid first deadline, impossible subsequent phase: reject atomically. */
    lapic_timer_init_mode(&t,UINT64_MAX-1536,true);
    assert(lapic_timer_write(&t,0x832,0x20040,UINT64_MAX-1536));
    assert(lapic_timer_write(&t,0x83e,11,UINT64_MAX-1536));
    assert(lapic_timer_write(&t,0x838,1,UINT64_MAX-1536));
    assert(t.deadline==UINT64_MAX-512);
    memcpy(&old,&t,sizeof old);
    vector=123;value=123;
    assert(lapic_timer_poll(&t,t.deadline,&vector)==LAPIC_TIMER_INVALID && vector==123);
    assert(memcmp(&old,&t,sizeof old)==0);
    assert(!lapic_timer_read(&t,0x839,t.deadline,&value) && value==123);
    assert(memcmp(&old,&t,sizeof old)==0);
    assert(lapic_timer_write(&t,0x838,0,UINT64_MAX)); /* safe stop remains possible */

    lapic_timer_init_mode(&t,0,true);
    assert(lapic_timer_write(&t,0x832,0x30040,0));
    assert(lapic_timer_write(&t,0x83e,10,0));
    assert(lapic_timer_write(&t,0x838,UINT32_MAX,0));
    memcpy(&old,&t,sizeof old);
    assert(lapic_timer_poll(&t,UINT64_MAX,&vector)==LAPIC_TIMER_INVALID);
    assert(memcmp(&old,&t,sizeof old)==0);
    lapic_timer_init_mode(&t,0,false);
    reject_write(&t,0x832,0x20040,0);
    lapic_timer_init(&t,0);
    assert(!t.periodic_supported);
    reject_write(&t,0x832,0x20040,0);
}

int main(void) {
    struct lapic_timer t;
    uint8_t vector=123;
    uint32_t ticks=123;
    lapic_timer_init(&t,100);
    assert(read_at(&t,0x832,100)==0x10000);
    assert(read_at(&t,0x838,100)==0);
    assert(read_at(&t,0x839,100)==0);
    assert(read_at(&t,0x83e,100)==0);
    assert(lapic_timer_reload(&t,100,0,&ticks)&&ticks==0);
    assert(lapic_timer_poll(&t,100,&vector)==LAPIC_TIMER_NONE && vector==123);
    reject_write(&t,0x838,1,100); /* vector not configured */
    reject_write(&t,0x832,31,100);
    reject_write(&t,0x832,0x20040,100); /* periodic */
    reject_write(&t,0x832,0x40040,100); /* deadline */
    reject_write(&t,0x832,0x100000040ULL,100);
    reject_write(&t,0x832,0x1040,100); /* delivery status bit not writable */
    reject_write(&t,0x83e,4,100);
    reject_write(&t,0x83e,0x10,100);
    reject_write(&t,0x839,1,100);
    reject_write(&t,0x800,1,100);
    assert(lapic_timer_write(&t,0x832,0x10040,100));

    const unsigned enc[8]={0,1,2,3,8,9,10,11};
    const unsigned div[8]={2,4,8,16,32,64,128,1};
    for(unsigned i=0;i<8;i++) {
        lapic_timer_init(&t,0);
        assert(lapic_timer_write(&t,0x832,0x10040,0));
        assert(lapic_timer_write(&t,0x83e,enc[i],0));
        assert(read_at(&t,0x83e,0)==enc[i]);
        uint64_t quantum=1024ULL*div[i],end=3*quantum;
        assert(lapic_timer_write(&t,0x838,3,0));
        assert(t.deadline==end && t.armed);
        assert(read_at(&t,0x839,0)==3);
        assert(read_at(&t,0x839,1)==3);
        assert(read_at(&t,0x839,quantum)==2);
        reject_write(&t,0x83e,enc[i],quantum);
        reject_write(&t,0x832,0x40,quantum);
        assert(read_at(&t,0x839,end-1)==1);
        assert(lapic_timer_poll(&t,end-1,&vector)==LAPIC_TIMER_NONE);
        assert(read_at(&t,0x839,end)==0); /* read does not consume expiry */
        vector=123;
        assert(lapic_timer_poll(&t,end,&vector)==LAPIC_TIMER_MASKED);
        assert(!t.armed && vector==123);
        assert(read_at(&t,0x838,end)==3); /* initial remains programmed */
        assert(lapic_timer_write(&t,0x832,0x40,end)); /* unmask after expiry */
        assert(lapic_timer_poll(&t,end+1,&vector)==LAPIC_TIMER_NONE && vector==123);
        assert(lapic_timer_write(&t,0x838,1,end+1));
        assert(lapic_timer_poll(&t,end+1+quantum,&vector)==LAPIC_TIMER_IRQ && vector==0x40);
        vector=123;
        assert(lapic_timer_poll(&t,end+2+quantum,&vector)==LAPIC_TIMER_NONE && vector==123);
    }

    lapic_timer_init(&t,0);
    assert(lapic_timer_write(&t,0x832,0xff,0));
    assert(lapic_timer_write(&t,0x83e,11,0)); /* /1 */
    assert(lapic_timer_write(&t,0x838,4,0));
    assert(lapic_timer_write(&t,0x838,2,100)); /* restart, not add */
    assert(t.deadline==2148 && t.initial==2);
    assert(lapic_timer_write(&t,0x838,0,101));
    assert(!t.armed && t.deadline==0 && t.initial==0);
    assert(lapic_timer_poll(&t,10000,&vector)==LAPIC_TIMER_NONE);
    reject_write(&t,0x838,UINT64_C(1)<<32,10000);
    reject_write(&t,0x838,1,9999);
    struct lapic_timer old;
    memcpy(&old,&t,sizeof old);
    uint64_t value=123;
    assert(!lapic_timer_read(&t,0x839,9999,&value)&&value==123);
    assert(!lapic_timer_read(&t,0x840,10001,&value)&&value==123);
    assert(lapic_timer_poll(&t,9999,&vector)==LAPIC_TIMER_INVALID);
    assert(memcmp(&old,&t,sizeof old)==0);
    ticks=123;
    assert(!lapic_timer_reload(&t,9999,0,&ticks)&&ticks==123);
    assert(!lapic_timer_reload(&t,10000,32,&ticks)&&ticks==123);
    lapic_timer_init(&t,UINT64_MAX-100);
    assert(lapic_timer_write(&t,0x832,0x40,UINT64_MAX-100));
    reject_write(&t,0x838,1,UINT64_MAX-100); /* deadline overflow */
    assert(lapic_timer_write(&t,0x838,0,UINT64_MAX)); /* cancelling is safe */

    lapic_timer_init(&t,0);
    assert(lapic_timer_write(&t,0x832,0x40,0));
    assert(lapic_timer_write(&t,0x83e,11,0));
    assert(lapic_timer_write(&t,0x838,1,0));
    assert(lapic_timer_reload(&t,0,0,&ticks)&&ticks==1024);
    assert(lapic_timer_reload(&t,1,5,&ticks)&&ticks==32);
    assert(lapic_timer_reload(&t,992,5,&ticks)&&ticks==1);
    assert(lapic_timer_reload(&t,1023,31,&ticks)&&ticks==1);
    assert(lapic_timer_reload(&t,1024,0,&ticks)&&ticks==0);
    assert(t.armed); /* reload does not consume expiry */
    assert(lapic_timer_write(&t,0x83e,0,0)==false); /* config while armed */
    assert(lapic_timer_write(&t,0x838,UINT32_MAX,0));
    assert(lapic_timer_reload(&t,0,0,&ticks)&&ticks==UINT32_MAX);
    assert(lapic_timer_reload(&t,0,10,&ticks)&&ticks==UINT32_MAX);

    uint64_t p=UINT64_C(1)<<6,e=UINT64_C(1)<<22;
    assert(lapic_timer_caps(p<<32,e<<32));
    assert(!lapic_timer_caps(0,e<<32));
    assert(!lapic_timer_caps(p<<32,0));
    assert(!lapic_timer_caps((p<<32)|p,e<<32));
    assert(!lapic_timer_caps(p<<32,(e<<32)|e));
    periodic_tests();
    puts("PASS one-shot/optional periodic LAPIC timer, phase, masks and expiry policy");
}
