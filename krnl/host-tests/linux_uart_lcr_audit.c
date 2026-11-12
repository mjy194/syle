#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
static void lcr(void){mock.f[0x6400]=0x3fb0000;mock.f[0x440c]=1;session.native.gpr[3]=0x3fb;}
int main(void){for(unsigned value=0;value<256;value++){setup(false);lcr();session.native.gpr[0]=value;unsigned effective=value&~0x30u;bool supported=effective==0||effective==3||effective==0x80||effective==0x83;assert(run(30)==supported);if(supported)assert(dispatch.uart.uart.lcr==value&&dispatch.ticks==1);else assert(dispatch.failed&&!dispatch.ticks&&dispatch.uart.uart.lcr==0);}
 setup(false);lcr();session.native.gpr[0]=3;mock.irqfail=true;assert(run(30)&&dispatch.uart.irq_dirty&&dispatch.uart.uart.lcr==3);
 setup(false);lcr();session.native.gpr[0]=3;mock.failwrite=4;assert(!run(30)&&dispatch.uart.uart.lcr==0);
 setup(false);lcr();session.native.gpr[3]=0x3fa;assert(!run(30));setup(false);lcr();mock.f[0x440c]=2;assert(!run(30));
 puts("UART LCR audit: exact DX byte path, 16/256 inactive-parity supported values; IRQ pending not rejection, badlen/DX/write stop PASS (host only)");return 0;}
