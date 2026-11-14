#include "../firmware_apic_guard.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void no(struct fag_state*s,uint64_t off,unsigned w,uint32_t value){struct fag_state old=*s;uint32_t save=value;assert(!fag_mmio(s,off,w,true,&value));assert(value==save&&!memcmp(s,&old,sizeof old));}
int main(void){for(unsigned pin=0;pin<256;pin++)assert(fag_host_events_exit(pin)==((pin&9)==9));assert(!fag_host_events_exit(UINT64_C(0x100000009)));struct fag_state s={0};assert(fag_init(&s)&&fag_valid(&s));assert(!fag_init(&s));
 uint64_t base=0;assert(fag_base(&s,false,&base)&&base==0xfee00900);assert(fag_base(&s,true,&base));
 base=0xfee00100;assert(!fag_base(&s,true,&base));base=0xfee00d00;assert(!fag_base(&s,true,&base));
 uint32_t v=99;assert(fag_mmio(&s,0x20,4,false,&v)&&v==0);assert(fag_mmio(&s,0x30,4,false,&v)&&v==0x60014);
 v=0x1ff;assert(fag_mmio(&s,0xf0,4,true,&v));v=0;assert(fag_mmio(&s,0x80,4,true,&v));
 v=0x10f;assert(fag_mmio(&s,0xf0,4,true,&v));
 v=0;assert(fag_mmio(&s,0xf0,4,false,&v)&&v==0x10f);
 no(&s,0xf0,4,0x10e);no(&s,0xf0,4,0x30f);no(&s,0xf0,4,0x110f);no(&s,0xf0,1,0x10f);
 v=0x10040;assert(fag_mmio(&s,0x320,4,true,&v));v=3;assert(fag_mmio(&s,0x3e0,4,true,&v));
 v=0;assert(fag_mmio(&s,0x380,4,true,&v));v=99;assert(fag_mmio(&s,0x390,4,false,&v)&&v==0);
 no(&s,0x380,4,1);no(&s,0x320,4,0x40040);no(&s,0x300,4,0x44040);no(&s,0x310,4,0);no(&s,0x3f0,4,0x40);
 no(&s,0x350,4,0x700);no(&s,0x360,4,0x400);no(&s,0x20,1,0);no(&s,0x81,4,0);no(&s,0x400,4,0);
 v=0;assert(fag_mmio(&s,0xb0,4,true,&v)); /* Empty EOI is an actual no-op. */
 v=0x20005;assert(fag_mmio(&s,0x320,4,true,&v)&&fag_valid(&s));
 v=0;assert(fag_mmio(&s,0x320,4,false,&v)&&v==0x20005);
 no(&s,0x380,4,0xffffffff);no(&s,0x320,1,0x20005);
 v=0;assert(fag_mmio(&s,0x380,4,true,&v));
 v=99;assert(fag_mmio(&s,0x390,4,false,&v)&&v==0);
 assert(fag_lint0_quiet_enable(&s));assert(!fag_lint0_quiet_enable(&s));
 v=0x700;assert(fag_mmio(&s,0x350,4,true,&v)&&fag_valid(&s));
 v=0;assert(fag_mmio(&s,0x350,4,false,&v)&&v==0x700);
 no(&s,0x350,4,0x701);no(&s,0x350,4,0x400);no(&s,0x360,4,0x400);
 no(&s,0x350,1,0x700);
 assert(fag_lint1_quiet_enable(&s));assert(!fag_lint1_quiet_enable(&s));
 v=0x400;assert(fag_mmio(&s,0x360,4,true,&v)&&fag_valid(&s));
 v=0;assert(fag_mmio(&s,0x360,4,false,&v)&&v==0x400);
 no(&s,0x360,4,0x401);no(&s,0x360,4,0x700);no(&s,0x360,4,0x500);no(&s,0x360,4,0x2400);no(&s,0x360,4,0x8400);no(&s,0x360,1,0x400);
 struct fag_state clean=s;
 s.apic.irq.isr[0]=1;assert(!fag_valid(&s));no(&s,0x80,4,0);s=clean;
 s.apic.tmr[0]=1;assert(!fag_valid(&s));no(&s,0x80,4,0);s=clean;
 s.apic.pending_level[0]=1;assert(!fag_valid(&s));no(&s,0x80,4,0);s=clean;
 s.apic.timer.armed=true;assert(!fag_valid(&s));no(&s,0x80,4,0);s=clean;
 s.apic.timer.initial=1;assert(!fag_valid(&s));no(&s,0x80,4,0);s=clean;
 s.apic.lvt[3]=0;assert(!fag_valid(&s));no(&s,0x80,4,0);s=clean;
 s.apic.irq.irr[1]=1;assert(!fag_valid(&s));no(&s,0x80,4,0);
 assert(!fag_init(0));puts("firmware APIC guard: fixed xAPIC/stopped timer/no events/atomic reject PASS");}
