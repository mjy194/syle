#include "nested_kick.h"
static uint64_t read_msr(uint32_t index) {
    uint32_t lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(index));
    return ((uint64_t)hi<<32)|lo;
}
static bool interrupts_disabled(void) {
    uint64_t flags;
    __asm__ volatile("pushfq;popq %0":"=r"(flags)::"memory");
    return !(flags&(1u<<9));
}
static volatile uint32_t *local_apic(uint64_t base) {
    /* This diagnostic deliberately only admits a low identity UC mapping.
     * Check the full architectural APIC-base address, not merely low 32 bits. */
    if(!interrupts_disabled() || !base || (base&4095) || base>=UINT64_C(0x100000000))
        return 0;
    uint64_t actual=read_msr(0x1b);
    if(!(actual&(1u<<11)) || (actual&(1u<<10)) ||
       (actual&UINT64_C(0x000ffffffffff000))!=base)return 0;
    return (volatile uint32_t *)(uintptr_t)base;
}
bool nested_kick_init(uint64_t base,unsigned expected_apicid) {
    volatile uint32_t *a=local_apic(base);
    if(!a || expected_apicid>255 || (a[0x20/4]>>24)!=expected_apicid)return false;
    unsigned max=(a[0x30/4]>>16)&255;
    /* These known layouts have timer, thermal, performance, LINT0/1, error;
     * max=6 additionally has CMCI. Unknown layouts are not guessed. */
    if(max<5 || max>6)return false;
    for(unsigned bank=0;bank<8;bank++)
        if(a[(0x100+bank*0x10)/4])return false;
    const unsigned lvt[]={0x320,0x330,0x340,0x350,0x360,0x370,0x2f0};
    unsigned count=max==6?7:6;
    for(unsigned i=0;i<count;i++)a[lvt[i]/4]=a[lvt[i]/4]|(1u<<16);
    a[0x80/4]=0;
    a[0xf0/4]=(a[0xf0/4]&~UINT32_C(0x1ff))|0x1ff;
    /* Volatile MMIO readbacks establish the programmed local state. */
    for(unsigned i=0;i<count;i++)if(!(a[lvt[i]/4]&(1u<<16)))return false;
    return (a[0x80/4]&255)==0 && (a[0xf0/4]&0x1ff)==0x1ff;
}
bool nested_kick_consume(uint64_t base,uint32_t exitinfo) {
    if(exitinfo!=UINT32_C(0x80000041))return false;
    volatile uint32_t *a=local_apic(base);
    if(!a)return false;
    /* EOI retires the highest ISR, not a supplied vector. Reject every other
     * ISR bit to avoid accidentally acknowledging unrelated host work. */
    for(unsigned bank=0;bank<8;bank++)
        if(a[(0x100+bank*0x10)/4]!=(bank==2?2u:0u))return false;
    a[0xb0/4]=0;
    return !(a[0x120/4]&2u);
}
