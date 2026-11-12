#include "../linux_uart_model_io.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool put(void*p,uint32_t f,uint64_t v){assert(f==0x681e&&v==0x1001);return !*(bool*)p;}
static void wr(struct linux_uart*u,unsigned r,uint8_t v){assert(linux_uart_access(u,r,true,&v));}
static uint8_t rd(struct linux_uart*u,unsigned r){uint8_t v=0;assert(linux_uart_access(u,r,false,&v));return v;}
int main(void){for(unsigned value=0;value<256;value++){
 struct linux_uart u;linux_uart_init(&u);struct linux_uart old=u;uint8_t v=value;unsigned frame=value&~0x30u;bool supported=frame==0||frame==3||frame==0x80||frame==0x83;
 assert(linux_uart_access(&u,3,true,&v)==supported);if(!supported){assert(!memcmp(&u,&old,sizeof u));continue;}
 assert(rd(&u,3)==value);if(value&128){wr(&u,0,0x34);wr(&u,1,0x12);assert(rd(&u,0)==0x34&&rd(&u,1)==0x12&&!u.tc&&!u.ier);}
 else {v=0xa5;assert(linux_uart_access(&u,0,true,&v)==(frame==3));if(frame==3){assert(u.tc==1);assert(linux_uart_drain(&u,&v)&&v==0xa5);wr(&u,4,16);wr(&u,0,0x5a);assert(rd(&u,0)==0x5a);}}
 }
 for(unsigned select=0;select<4;select++){struct linux_uart u;linux_uart_init(&u);wr(&u,3,0x83|(select<<4));wr(&u,0,1);wr(&u,1,0);wr(&u,3,3|(select<<4));wr(&u,1,0);wr(&u,0,'X');uint8_t v;assert(linux_uart_drain(&u,&v)&&v=='X');
 uint64_t g[15]={0};g[0]=0x93;g[3]=0x3fb;struct linux_uart old=u;bool fail=true,irq=false;assert(!lumio_apply(&u,g,0x1000,0,30,0x3fb0000,1,&fail,put,&irq)&&!memcmp(&u,&old,sizeof u));fail=false;assert(lumio_apply(&u,g,0x1000,0,30,0x3fb0000,1,&fail,put,&irq)&&u.lcr==0x93);
 }
 puts("UART LCR semantic matrix: inactive parity selectors retained, effective 8N1 TX/loopback and DLAB isolated; 256 values PASS");return 0;}
