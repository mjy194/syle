#include "../guest_apic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct guest_apic_effect wr(struct guest_apic *a,uint32_t m,uint64_t v,uint64_t now) {
    struct guest_apic_effect e={0};assert(guest_apic_write(a,m,v,now,&e));return e;
}
static uint64_t rd(struct guest_apic *a,uint32_t m,uint64_t now) {
    uint64_t v;assert(guest_apic_read(a,m,now,&v));return v;
}
static void bad(struct guest_apic *a,uint32_t m,uint64_t v,uint64_t now) {
    struct guest_apic b;memcpy(&b,a,sizeof b);
    struct guest_apic_effect e={.eoi=true,.level=true,.vector=123};
    assert(!guest_apic_write(a,m,v,now,&e));
    assert(memcmp(a,&b,sizeof b)==0 && e.eoi && e.level && e.vector==123);
}
int main(void) {
    struct guest_apic a;guest_apic_init(&a,0);
    assert(rd(&a,0x1b,0)==0xfee00d00 && rd(&a,0x803,0)==0x60014);
    assert(rd(&a,0x802,0)==0 && rd(&a,0x80d,0)==1);
    uint64_t out=123;assert(!guest_apic_read(&a,0x80e,0,&out) && out==123);
    for(unsigned m=0x833;m<=0x837;m++) {
        assert(rd(&a,m,0)==0x10000);
        wr(&a,m,0x10040,0);bad(&a,m,0x40,0);
    }
    wr(&a,0x835,0x18700,0); /* masked ExtINT permitted as register state */
    bad(&a,0x835,0x8700,0); /* live ExtINT unsupported */
    wr(&a,0x836,0x10400,0);bad(&a,0x836,0x400,0); /* NMI unsupported */
    bad(&a,0x837,0x10400,0); /* error LVT delivery mode reserved */
    assert(!guest_apic_enqueue(&a,64,true));
    wr(&a,0x80f,0x1ff,0);
    assert(guest_apic_enqueue(&a,64,true));
    assert(guest_apic_enqueue(&a,64,true));
    assert(!guest_apic_enqueue(&a,64,false));
    uint8_t v=0;assert(guest_apic_select(&a,&v) && v==64);
    assert(rd(&a,0x81a,0)==1); /* trigger remembered when queued */
    assert(guest_apic_accept(&a,v));assert(rd(&a,0x81a,0)==1);
    assert(!guest_apic_enqueue(&a,64,false)); /* cannot mix trigger in flight */
    assert(guest_apic_enqueue(&a,64,true));
    struct guest_apic_effect e=wr(&a,0x80b,0,0);
    assert(e.eoi && e.level && e.vector==64 && rd(&a,0x81a,0)==1);
    assert(guest_apic_accept(&a,64));e=wr(&a,0x80b,0,0);
    assert(e.eoi && e.level && e.vector==64 && rd(&a,0x81a,0)==0);
    e=wr(&a,0x80b,0,0);assert(!e.eoi);
    wr(&a,0x830,0x40050,0); /* self */
    assert(rd(&a,0x830,0)==0x40050);
    assert(guest_apic_accept(&a,80));wr(&a,0x80b,0,0);
    wr(&a,0x830,0x84051,0); /* all including self, assert */
    assert(guest_apic_accept(&a,81));wr(&a,0x80b,0,0);
    wr(&a,0x83f,82,0);assert(guest_apic_accept(&a,82));wr(&a,0x80b,0,0);
    bad(&a,0x830,0xc0050,0);bad(&a,0x830,0x40550,0); /* excluding, INIT */
    bad(&a,0x830,UINT64_C(1)<<32|0x50,0); /* non-BSP */
    bad(&a,0x1b,0xfee00900,0);bad(&a,0x828,1,0);
    wr(&a,0x832,0x10000,1);wr(&a,0x838,1,1);
    assert(guest_apic_poll(&a,2049)); /* masked low vector does not emit */
    wr(&a,0x832,0x20060,2049);wr(&a,0x83e,11,2049);wr(&a,0x838,1,2049);
    assert(guest_apic_poll(&a,3073));assert(guest_apic_select(&a,&v) && v==96);
    assert(guest_apic_accept(&a,v));e=wr(&a,0x80b,0,3073);
    assert(e.eoi && !e.level && e.vector==96);
    bad(&a,0x832,0x10000,4097); /* due event must be polled first */
    assert(guest_apic_poll(&a,4097));wr(&a,0x832,0x10000,4097);
    bad(&a,0x80f,0x1ff,4096); /* monotonic all-access clock */
    out=123;assert(!guest_apic_read(&a,0x802,4096,&out) && out==123);
    /* xAPIC MMIO and transition share the same priority/timer model. */
    guest_apic_init_mode(&a,0,false);
    assert(rd(&a,0x1b,0)==0xfee00900);
    out=123;assert(!guest_apic_read(&a,0x803,0,&out) && out==123);
    uint32_t mm=123;e=(struct guest_apic_effect){0};
    assert(guest_apic_mmio(&a,0x30,4,false,&mm,0,&e) && mm==0x60014);
    mm=0x1ff;assert(guest_apic_mmio(&a,0xf0,4,true,&mm,0,&e));
    mm=0x01000000;assert(guest_apic_mmio(&a,0xd0,4,true,&mm,0,&e));
    mm=0;assert(guest_apic_mmio(&a,0xd0,4,false,&mm,0,&e) && mm==0x01000000);
    assert(guest_apic_mmio(&a,0xe0,4,false,&mm,0,&e) && mm==UINT32_MAX);
    mm=0x40040;assert(guest_apic_mmio(&a,0x300,4,true,&mm,0,&e));
    assert(guest_apic_accept(&a,64));
    mm=0;assert(guest_apic_mmio(&a,0xb0,4,true,&mm,0,&e) && e.eoi && e.vector==64);
    struct guest_apic before;memcpy(&before,&a,sizeof a);
    mm=0xdeadbeef;e=(struct guest_apic_effect){.eoi=true,.level=true,.vector=123};
    assert(!guest_apic_mmio(&a,0x31,4,false,&mm,0,&e));
    assert(!guest_apic_mmio(&a,0x30,8,false,&mm,0,&e));
    assert(!guest_apic_mmio(&a,0x310,4,true,&mm,0,&e));
    assert(!guest_apic_mmio(&a,0x3f0,4,true,&mm,0,&e));
    assert(memcmp(&a,&before,sizeof a)==0 && mm==0xdeadbeef && e.vector==123 && e.eoi);
    wr(&a,0x1b,0xfee00d00,0);
    assert(rd(&a,0x80f,0)==0x1ff && rd(&a,0x80d,0)==1);
    assert(!guest_apic_mmio(&a,0x30,4,false,&mm,0,&e));
    puts("PASS opt-in guest APIC registers, LVT policy, ICR, TMR/EOI, timer phase");
}
