#include "../linux_uart.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
static void wr(struct linux_uart*u,unsigned reg,uint8_t v){assert(linux_uart_access(u,reg,true,&v));}
static uint8_t rd(struct linux_uart*u,unsigned reg){uint8_t v=0;assert(linux_uart_access(u,reg,false,&v));return v;}
static bool level(void*p,bool v){*(bool*)p=v;return true;}
int main(void){struct linux_uart u;linux_uart_init(&u);bool accepted,irq=false;uint8_t b;
 wr(&u,3,3);wr(&u,4,8);wr(&u,2,1);wr(&u,1,7);assert(linux_uart_irq_level(&u));assert(rd(&u,2)==0xc2);assert(!linux_uart_irq_level(&u));
 for(unsigned i=0;i<16;i++){assert(linux_uart_receive(&u,(uint8_t)i,&accepted)&&accepted);}
 assert(linux_uart_receive(&u,99,&accepted)&&!accepted);assert(rd(&u,2)==0xc6);assert(rd(&u,5)==0x63);assert(rd(&u,2)==0xc4);
 for(unsigned i=0;i<16;i++){assert(rd(&u,0)==i);}
assert(!linux_uart_irq_level(&u));
 for(unsigned i=0;i<16;i++){wr(&u,0,(uint8_t)(i+32));}
struct linux_uart saved=u;b=99;assert(!linux_uart_access(&u,0,true,&b));assert(!memcmp(&u,&saved,sizeof u)&&b==99);
 assert(!(rd(&u,5)&0x60));for(unsigned i=0;i<16;i++){assert(linux_uart_drain(&u,&b)&&b==i+32);}assert(linux_uart_irq_level(&u));assert(rd(&u,2)==0xc2);assert(!linux_uart_irq_level(&u));
 wr(&u,2,0x41);for(unsigned i=0;i<3;i++)assert(linux_uart_receive(&u,42,&accepted)&&accepted);assert(!linux_uart_irq_level(&u));assert(linux_uart_receive(&u,43,&accepted)&&accepted);assert(linux_uart_irq_level(&u));assert(linux_uart_publish_irq(&u,&irq,level)&&irq);assert(rd(&u,0)==42);assert(!linux_uart_irq_level(&u));
 wr(&u,2,7);assert(u.rc==0&&u.tc==0);wr(&u,3,0x83);wr(&u,0,12);wr(&u,1,0);assert(rd(&u,0)==12);wr(&u,3,3);
 saved=u;b=16;assert(!linux_uart_access(&u,1,true,&b));assert(!memcmp(&u,&saved,sizeof u));b=0x20;assert(!linux_uart_access(&u,4,true,&b));assert(!memcmp(&u,&saved,sizeof u));
 wr(&u,2,0);assert(linux_uart_receive(&u,1,&accepted)&&accepted);assert(linux_uart_receive(&u,2,&accepted)&&!accepted);assert(rd(&u,0)==1);

 linux_uart_init(&u);wr(&u,3,3);wr(&u,2,0x41);wr(&u,1,1);
 assert(linux_uart_receive(&u,65,&accepted)&&accepted);assert(linux_uart_tick(&u,3));assert((rd(&u,2)&15)==1);
 assert(linux_uart_tick(&u,1));assert((rd(&u,2)&15)==12);assert(!linux_uart_irq_level(&u));wr(&u,4,8);assert(linux_uart_irq_level(&u));assert(rd(&u,0)==65);assert(!linux_uart_irq_level(&u));
 wr(&u,1,8);assert(linux_uart_modem_input(&u,0xb0));assert(linux_uart_irq_level(&u));assert((rd(&u,2)&15)==0);assert(rd(&u,6)==0xbb);assert(!linux_uart_irq_level(&u));
 wr(&u,4,0x1f);assert(rd(&u,6)==0xf0);wr(&u,0,0x5a);assert(rd(&u,0)==0x5a);assert(!linux_uart_drain(&u,&b));
 wr(&u,4,0x1b);assert(rd(&u,6)==0xb4);assert(!linux_uart_receive(&u,7,&accepted));
 wr(&u,4,8);assert(rd(&u,6)==0xb0);
 linux_uart_init(&u);wr(&u,3,3);wr(&u,4,8);wr(&u,1,1);wr(&u,2,0x41);
 assert(linux_uart_receive(&u,1,&accepted)&&accepted);assert(linux_uart_tick(&u,UINT_MAX));assert(u.rx_idle==4);
 assert((rd(&u,2)&15)==12);assert((rd(&u,2)&15)==12);
 assert(linux_uart_receive(&u,2,&accepted)&&accepted);assert(u.rx_idle==0&&(rd(&u,2)&15)==1);
 assert(linux_uart_tick(&u,4));assert(rd(&u,0)==1);assert(u.rx_idle==0);
 wr(&u,0,3);saved=u;b=0x18;assert(!linux_uart_access(&u,4,true,&b));assert(!memcmp(&u,&saved,sizeof u));
 puts("linux_uart: PASS (bounded RX/TX/modem/IRQ/timeout pure model; not wired)");}
