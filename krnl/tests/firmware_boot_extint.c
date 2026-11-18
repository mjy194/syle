#include "../firmware_apic_guard.h"
#include <assert.h>
#include <stdio.h>
int main(void){struct fag_state d={0};assert(fag_init(&d));assert(!fag_lint0_route_enable(&d));assert(fag_lint0_quiet_enable(&d));assert(!fag_lint0_route_enable(&d));uint32_t v=0x700;assert(fag_mmio(&d,0x350,4,true,&v));assert(!fag_lint0_route_enable(&d));v=0x1ff;assert(fag_mmio(&d,0xf0,4,true,&v));assert(fag_lint0_route_enable(&d));assert(d.lint0_extint_routed&&!d.lint0_extint_deasserted&&fag_valid(&d));assert(!fag_lint0_route_enable(&d));assert(!fag_lint0_quiet_enable(&d));v=0x700;assert(fag_mmio(&d,0x350,4,true,&v));struct fag_state bad=d;bad.lint0_extint_deasserted=true;assert(!fag_valid(&bad));bad=d;bad.apic.irq.irr[0]=1;assert(!fag_valid(&bad));puts("exclusive quiet-to-ExtINT route transition PASS");}
