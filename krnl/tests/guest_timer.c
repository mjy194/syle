#include "../lapic_timer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void wr(struct lapic_timer *t,uint64_t off,uint32_t v,uint64_t now) {
    assert(lapic_timer_mmio(t,off,4,true,&v,now));
}
static uint32_t rd(struct lapic_timer *t,uint64_t off,uint64_t now) {
    uint32_t v=0xdeadbeef;
    assert(lapic_timer_mmio(t,off,4,false,&v,now));
    return v;
}
static void reject(struct lapic_timer *t,uint64_t off,unsigned width,
                   bool write,uint32_t v,uint64_t now) {
    struct lapic_timer old;
    memcpy(&old,t,sizeof old);
    uint32_t original=v;
    assert(!lapic_timer_mmio(t,off,width,write,&v,now));
    assert(v==original && memcmp(&old,t,sizeof old)==0);
}
int main(void) {
    struct lapic_timer t;
    uint8_t vector=0;
    lapic_timer_init_guest(&t,0);
    assert(t.guest_profile && t.periodic_supported);
    wr(&t,0x320,0x20040,0);wr(&t,0x3e0,11,0);wr(&t,0x380,4,0);
    assert(t.deadline==4096 && t.period==4096);
    /* Guest masks a running periodic timer, changes vector, and unmasks.
     * Neither operation restarts it or loses the periodic phase. */
    wr(&t,0x320,0x30041,1024);
    assert(rd(&t,0x390,1024)==3 && t.deadline==4096);
    assert(lapic_timer_poll(&t,4096,&vector)==LAPIC_TIMER_MASKED);
    assert(t.deadline==8192 && t.armed);
    wr(&t,0x320,0x20041,4096);
    assert(t.deadline==8192 && t.period==4096);
    assert(lapic_timer_poll(&t,8192,&vector)==LAPIC_TIMER_IRQ && vector==0x41);
    /* Mode switch preserves current cycle; periodic -> one-shot disarms
     * at next expiry, one-shot -> periodic repeats from the existing phase. */
    wr(&t,0x320,0x42,8193);
    assert(t.deadline==12288);
    assert(lapic_timer_poll(&t,12288,&vector)==LAPIC_TIMER_IRQ && vector==0x42);
    assert(!t.armed);
    wr(&t,0x380,2,12288);
    wr(&t,0x320,0x20043,13000);
    assert(t.deadline==14336 && t.period==2048);
    assert(lapic_timer_poll(&t,14336,&vector)==LAPIC_TIMER_IRQ && vector==0x43);
    assert(t.armed && t.deadline==16384);
    /* Due events must be consumed with their OLD vector/mask before config. */
    reject(&t,0x320,4,true,0x30044,16384);
    assert(lapic_timer_poll(&t,16384,&vector)==LAPIC_TIMER_IRQ && vector==0x43);
    wr(&t,0x320,0x30044,16384);
    reject(&t,0x320,4,true,0x40044,16384); /* no deadline mode */
    reject(&t,0x320,4,true,31,16384);
    reject(&t,0x320,4,true,0x80044,16384);
    reject(&t,0x3e0,4,true,0,16384); /* live divider not implemented */
    reject(&t,0x390,4,true,1,16384);
    reject(&t,0x321,4,false,123,16384);
    reject(&t,0x320,8,false,123,16384);
    reject(&t,0x330,4,false,123,16384);
    reject(&t,0x320,4,false,123,16383);
    wr(&t,0x380,0,16384);
    wr(&t,0x3e0,0,16384);
    assert(rd(&t,0x390,16384)==0);
    assert(rd(&t,0x320,16384)==0x30044);
    assert(rd(&t,0x3e0,16384)==0);
    /* The MMIO alias operates on the same state as x2APIC MSRs. */
    assert(lapic_timer_write(&t,0x832,0x45,16384));
    assert(rd(&t,0x320,16384)==0x45);
    wr(&t,0x380,1,16384);
    uint64_t count=0;
    assert(lapic_timer_read(&t,0x838,16384,&count) && count==1);
    assert(lapic_timer_poll(&t,18432,&vector)==LAPIC_TIMER_IRQ && vector==0x45);
    uint32_t v=0;
    assert(!lapic_timer_mmio(NULL,0x320,4,false,&v,0));
    assert(!lapic_timer_mmio(&t,0x320,4,false,NULL,18432));
    puts("PASS guest timer live LVT/phase, xAPIC-MMIO aliases and rejection atomicity");
}
