#include "../lapic.h"
#include "../ept.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t read_reg(struct lapic *a, uint32_t msr) {
    uint64_t value = UINT64_MAX;
    assert(lapic_read(a, msr, &value));
    return value;
}
static void reject_write(struct lapic *a, uint32_t msr, uint64_t value) {
    struct lapic before = *a;
    assert(!lapic_write(a, msr, value));
    assert(memcmp(a, &before, sizeof before) == 0);
}
static void reject_read(struct lapic *a, uint32_t msr) {
    uint64_t value = UINT64_C(0x123456789abcdef0);
    assert(!lapic_read(a, msr, &value));
    assert(value == UINT64_C(0x123456789abcdef0));
}
static void initialize(struct lapic *a) {
    lapic_init(a);
    assert(lapic_write(a, 0x1b, 0xfee00d00));
    assert(lapic_write(a, 0x80f, 0x1ff));
}
int main(void) {
    assert(lapic_irq_ack_valid(31,0x6123,0x6123,0));
    assert(!lapic_irq_ack_valid(52,0x6123,0x6123,0)); /* timer during injection */
    assert(!lapic_irq_ack_valid(7,0x6123,0x6123,0));
    assert(!lapic_irq_ack_valid(31|UINT64_C(0x80000000),0x6123,0x6123,0));
    assert(!lapic_irq_ack_valid(31,0x6124,0x6123,0));
    assert(!lapic_irq_ack_valid(31,0x6123,0x6123,UINT64_C(0x80000040)));

    struct lapic a;
    lapic_init(&a);
    assert(read_reg(&a, 0x1b) == 0xfee00900);
    reject_read(&a, 0x802);
    reject_write(&a, 0x808, 0);
    reject_write(&a, 0x1b, 0xfee00500); /* cannot skip enabled state */
    reject_write(&a, 0x1b, 0xfee00c00); /* BSP immutable */
    assert(lapic_write(&a, 0x1b, 0xfee00900));
    assert(lapic_write(&a, 0x1b, 0xfee00d00));
    reject_write(&a, 0x1b, 0xfee00900);
    reject_write(&a, 0x1b, 0xfee00100);
    reject_write(&a, 0x1b, 0xfee01d00);
    assert(read_reg(&a, 0x802) == 0);
    assert(read_reg(&a, 0x803) == 0x14);
    assert(read_reg(&a, 0x80f) == 0xff);
    reject_write(&a, 0x83f, 0x40); /* software disabled */
    assert(lapic_write(&a, 0x80f, 0x1ff));
    reject_write(&a, 0x80f, 0x11f);
    reject_write(&a, 0x80f, 0x11ff);
    reject_write(&a, 0x808, 0x100);
    reject_write(&a, 0x80b, 1);
    reject_write(&a, 0x83f, 31);
    reject_write(&a, 0x83f, 256);
    reject_write(&a, 0x802, 1);
    reject_write(&a, 0x803, 1);
    reject_write(&a, 0x80a, 0);
    reject_write(&a, 0x810, 0);
    reject_write(&a, 0x820, 0);
    reject_write(&a, 0x830, 0); /* ICR not implemented */
    reject_read(&a, 0x80b);
    reject_read(&a, 0x83f);
    reject_read(&a, 0x830);
    reject_read(&a, 0x10000000);

    uint8_t selected = 0xee;
    assert(!lapic_select(&a, &selected) && selected == 0xee);
    assert(lapic_write(&a, 0x83f, 0x40));
    assert(lapic_write(&a, 0x83f, 0x40));
    assert(read_reg(&a, 0x822) == 1);
    struct lapic before = a;
    assert(lapic_select(&a, &selected) && selected == 0x40);
    assert(memcmp(&a, &before, sizeof a) == 0); /* VMfail preserves pending */
    assert(!lapic_accept(&a, 0x41));
    assert(memcmp(&a, &before, sizeof a) == 0);
    assert(lapic_accept(&a, 0x40));
    assert(read_reg(&a, 0x822) == 0 && read_reg(&a, 0x812) == 1);
    assert(lapic_ppr(&a) == 0x40);

    assert(lapic_write(&a, 0x808, 0x4f));
    assert(lapic_ppr(&a) == 0x4f); /* equality preserves TPR low nibble */
    assert(lapic_write(&a, 0x83f, 0x4f));
    assert(!lapic_select(&a, &selected)); /* same class blocked */
    assert(lapic_write(&a, 0x808, 0x2f));
    assert(lapic_ppr(&a) == 0x40); /* ISR wins, low nibble zero */
    assert(!lapic_select(&a, &selected));
    assert(lapic_write(&a, 0x83f, 0x60));
    assert(lapic_select(&a, &selected) && selected == 0x60);
    assert(lapic_accept(&a, selected)); /* nested higher class */
    assert(lapic_ppr(&a) == 0x60);
    assert(read_reg(&a, 0x813) == 1 && read_reg(&a, 0x812) == 1);
    assert(lapic_write(&a, 0x80b, 0)); /* clears highest ISR only */
    assert(read_reg(&a, 0x813) == 0 && read_reg(&a, 0x812) == 1);
    assert(lapic_ppr(&a) == 0x40);
    assert(lapic_write(&a, 0x80b, 0));
    assert(lapic_ppr(&a) == 0x2f);
    assert(lapic_select(&a, &selected) && selected == 0x4f);
    assert(lapic_accept(&a, selected));
    assert(lapic_write(&a, 0x80b, 0));
    assert(lapic_write(&a, 0x80b, 0)); /* no ISR no-op */

    initialize(&a);
    assert(lapic_write(&a, 0x83f, 255));
    assert(read_reg(&a, 0x827) == UINT32_C(0x80000000));
    assert(lapic_write(&a, 0x808, 0xff));
    assert(!lapic_select(&a, &selected));
    assert(lapic_write(&a, 0x808, 0xef));
    assert(lapic_select(&a, &selected) && selected == 255);
    assert(lapic_accept(&a, selected));
    assert(read_reg(&a, 0x817) == UINT32_C(0x80000000));
    assert(lapic_write(&a, 0x83f, 255)); /* same vector pending while in service */
    assert(!lapic_select(&a, &selected));
    assert(lapic_write(&a, 0x80b, 0));
    assert(lapic_select(&a, &selected));
    assert(lapic_write(&a, 0x80f, 0xff));
    assert(!lapic_select(&a, &selected)); /* disabling software gates delivery */
    assert(lapic_window_caps(UINT64_C(4)<<32));
    assert(!lapic_window_caps(0));
    assert(!lapic_window_caps((UINT64_C(4)<<32)|4));
    lapic_init(&a);
    assert(lapic_write(&a,0x1b,0xfee00d00));
    assert(lapic_write(&a,0x80f,0x1ff));
    assert(!lapic_window_arm(&a,false,false,0,0)); /* no pending */
    assert(lapic_write(&a,0x83f,0x40));
    assert(lapic_window_arm(&a,false,false,0,0));
    assert(!lapic_window_arm(&a,true,false,0,0));
    assert(!lapic_window_arm(&a,false,true,0,0));
    assert(!lapic_window_arm(&a,false,false,1ULL<<31,0));
    assert(!lapic_window_arm(&a,false,false,0,1));
    assert(lapic_window_ready(&a,7,0x202,0,0,0,0,&selected) && selected==0x40);
    for(unsigned i=0;i<6;i++) {
        uint64_t args[6]={7,0x202,0,0,0,0};
        args[i]^=(i==4||i==5)?(1ULL<<31):(i==1?0x200:1);
        assert(!lapic_window_ready(&a,args[0],args[1],args[2],args[3],args[4],args[5],&selected));
    }
    assert(!lapic_window_ready(&a,7,0x202,2,0,0,0,&selected)); /* MOVSS */
    assert(lapic_write(&a,0x808,0x40));
    assert(!lapic_window_arm(&a,false,false,0,0)); /* same-class PPR */
    assert(!lapic_window_ready(&a,7,0x202,0,0,0,0,&selected));
    assert(read_reg(&a,0x822)==1); /* all window checks are non-consuming */
    assert(ept_movss_fault_valid(48,0x5aa,0xb080,1,1,2));
    assert(!ept_movss_fault_valid(48,0x5aa,0xb080,1,1,0));
    assert(!ept_movss_fault_valid(48,0x5aa,0xb080,1,1,1));
    assert(!ept_movss_fault_valid(48,0x58a,0xb080,1,1,2));
    assert(!ept_movss_fault_valid(0,0x5aa,0xb080,1,1,2));
    assert(!ept_movss_fault_valid(48,0x5aa,0xb000,1,1,2));
    assert(!ept_movss_fault_valid(48,0x5aa,0xb080,2,1,2));
    assert(lapic_write(&a,0x808,0));
    assert(lapic_collision_ud_ready(&a,0,0x80000306,2,0x202,0,0,0,false));
    for(unsigned i=0;i<7;i++) {
        uint64_t args[7]={0,0x80000306,2,0x202,0,0,0};
        args[i]^=(i==5||i==6)?(1ULL<<31):(i==3?0x200:1);
        assert(!lapic_collision_ud_ready(&a,args[0],args[1],args[2],args[3],args[4],args[5],args[6],false));
    }
    assert(!lapic_collision_ud_ready(&a,0,0x80000306,2,0x302,0,0,0,false)); /* TF */
    assert(!lapic_collision_ud_ready(&a,0,0x80000306,2,0x202,0,0,0,true));
    assert(!lapic_window_ready(&a,7,0x202,2,0,0,0,&selected));
    assert(read_reg(&a,0x822)==1 && read_reg(&a,0x812)==0);
    puts("PASS limited x2APIC register, priority, EOI and pending transaction policy");
}
