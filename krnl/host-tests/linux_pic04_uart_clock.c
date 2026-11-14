#include "linux_pic04_domain.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool wr(void*p,uint32_t f,uint64_t v){(void)v;assert(f==0x681e);return !p;}
static void io(struct lpd_domain*d,unsigned reg,bool write,uint8_t value){uint64_t g[15]={value};g[3]=0x3f8+reg;assert(lpd_uart_io(d,g,0x1000,((uint64_t)g[3]<<16)|(write?0:8),1,0,wr));}
static void setup(struct lpd_domain*d,unsigned divisor,uint8_t lcr){assert(lpd_init(d,1));io(d,3,true,128|lcr);io(d,0,true,divisor);io(d,1,true,divisor>>8);io(d,3,true,lcr);io(d,2,true,0xc1);io(d,1,true,1);io(d,4,true,8);}
static void receive(struct lpd_domain*d){bool a=false;assert(lpd_send(d,65,&a)&&a);}
static uint64_t deadline(unsigned divisor,unsigned halfbits){uint64_t n=UINT64_C(1193182)*divisor*halfbits*4;return n/230400+(n%230400!=0);}
int main(void){struct lpd_domain d;uint64_t end;
 for(unsigned div=1;div<=65535;div=div==1?12:div==12?65535:65536){
  for(unsigned lcr=0;lcr<64;lcr++){unsigned data=5+(lcr&3),bits=2+2*data+((lcr&8)?2:0)+((lcr&4)?(data==5?3:4):2);
   setup(&d,div,lcr);receive(&d);end=deadline(div,bits);
   assert(lpd_tick(&d,end-1)&&!d.master.level4&&d.uart.uart.rx_idle<4);
   assert(lpd_tick(&d,end)&&d.master.level4&&d.uart.uart.rx_idle==4);
  }
 }
 /* Split fractional intervals produce precisely the same timeout. */
 setup(&d,1,3);receive(&d);end=deadline(1,20);for(uint64_t t=1;t<end;t++)assert(lpd_tick(&d,t)&&!d.master.level4);
 assert(lpd_tick(&d,end)&&d.master.level4);
 /* New RX and RBR activity reset both whole and fractional character age. */
 setup(&d,1,3);receive(&d);assert(lpd_tick(&d,end-1));receive(&d);assert(d.uart_fraction==0&&d.uart.uart.rx_idle==0);assert(lpd_tick(&d,2*end-2)&&!d.master.level4);
 io(&d,0,false,0);assert(d.uart.uart.rc==1&&d.uart_fraction==0&&d.uart.uart.rx_idle==0);
 assert(lpd_tick(&d,3*end-3)&&!d.master.level4);assert(lpd_tick(&d,3*end-2)&&d.master.level4);
 /* LCR and divisor changes restart timing, DLAB alone doesn't. */
 io(&d,3,true,0x83);assert(d.uart.uart.rx_idle==4);io(&d,0,true,12);assert(d.uart.uart.rx_idle==0&&!d.master.level4);io(&d,3,true,3);
 uint64_t base=d.timer.tick;end=deadline(12,20);assert(lpd_tick(&d,base+end-1)&&!d.master.level4);assert(lpd_tick(&d,base+end)&&d.master.level4);
 io(&d,3,true,7);assert(!d.master.level4&&d.uart.uart.rx_idle==0);
 /* FCR clear, then huge delta cannot overflow or create empty-RX timeout. */
 io(&d,2,true,0xc3);assert(!d.uart.uart.rc&&!d.uart_fraction);assert(lpd_tick(&d,UINT64_MAX)&&!d.master.level4);
 setup(&d,65535,63);receive(&d);assert(lpd_tick(&d,UINT64_MAX)&&d.master.level4);
 struct lpd_domain old=d;assert(!lpd_tick(&d,UINT64_MAX-1)&&!memcmp(&d,&old,sizeof d));
 /* Divisor zero is unclocked; no invented baud. */
 setup(&d,0,3);receive(&d);assert(lpd_tick(&d,UINT64_MAX)&&!d.master.level4&&!d.uart_fraction);
 /* A failed VMWRITE must not reset RX age/configuration. */
 setup(&d,1,3);receive(&d);assert(lpd_tick(&d,100));old=d;uint64_t g[15]={7};g[3]=0x3fb;assert(!lpd_uart_io(&d,g,0x1000,UINT64_C(0x3fb0000),1,&d,wr)&&!memcmp(&d,&old,sizeof d));
 puts("PIC04 UART clock: fractional frames/timeouts/activity/overflow/atomic failure PASS");
}
