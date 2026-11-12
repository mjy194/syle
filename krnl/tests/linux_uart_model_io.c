#include "../linux_uart_model_io.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct mock{unsigned calls;bool fail;};
static bool put(void*p,uint32_t f,uint64_t v){struct mock*m=p;assert(f==0x681e&&v==0x1001);m->calls++;return !m->fail;}
static void wr(struct linux_uart*u,unsigned r,uint8_t v){assert(linux_uart_access(u,r,true,&v));}
static void failed_read(struct linux_uart*u,unsigned r){struct linux_uart old=*u;uint64_t g[15]={0},saved[15];g[0]=0x123456789abcdef0ULL;g[3]=0x3f8+r;memcpy(saved,g,sizeof g);bool irq=true;struct mock m={.fail=true};assert(!lumio_apply(u,g,0x1000,0,30,((0x3f8ULL+r)<<16)|8,1,&m,put,&irq));assert(m.calls==1&&!memcmp(u,&old,sizeof old)&&!memcmp(g,saved,sizeof g)&&irq);}
int main(void){struct linux_uart u;linux_uart_init(&u);wr(&u,3,3);wr(&u,4,8);wr(&u,1,15);bool accepted,irq=false;uint64_t g[15]={0};struct mock m={0};
 assert(linux_uart_receive(&u,0x41,&accepted)&&accepted);assert(linux_uart_receive(&u,0x42,&accepted)&&!accepted);
 failed_read(&u,0); /* RX pop */
 failed_read(&u,5); /* OE clear */
 assert(linux_uart_modem_input(&u,0xb0));failed_read(&u,6); /* modem delta clear */
 wr(&u,2,7); /* clear RX/OE; TX interrupt pending */
 uint8_t dummy=0;assert(linux_uart_access(&u,6,false,&dummy));failed_read(&u,2); /* TX IIR acknowledgement */
 g[0]=0x1122334455667788ULL;g[3]=0x3fa;
 assert(lumio_apply(&u,g,0x1000,0,30,0x3fa0008,1,&m,put,&irq));assert(g[0]==0x11223344556677c2ULL&&!irq&&!u.tx_pending);
 wr(&u,2,0);g[0]=0x55;g[3]=0x3f8;m=(struct mock){.fail=true};struct linux_uart old=u;uint64_t saved[15];memcpy(saved,g,sizeof g);irq=true;
 assert(!lumio_apply(&u,g,0x1000,0,30,0x3f80000,1,&m,put,&irq));assert(!memcmp(&old,&u,sizeof u)&&!memcmp(saved,g,sizeof g)&&irq);
 wr(&u,0,1);old=u;m=(struct mock){0};assert(!lumio_apply(&u,g,0x1000,0,30,0x3f80000,1,&m,put,&irq));assert(!m.calls&&!memcmp(&old,&u,sizeof u));
 const uint64_t bad[]={0x3f80001,0x3f80010,0x3f80020,0x3f80040,0x3f90000,0x100003f80000ULL};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){m=(struct mock){0};assert(!lumio_apply(&u,g,0x1000,0,30,bad[i],1,&m,put,&irq));assert(!m.calls&&!memcmp(&old,&u,sizeof u));}
 m=(struct mock){0};assert(!lumio_apply(&u,g,0x1000,3,30,0x3f80008,1,&m,put,&irq));assert(!lumio_apply(&u,g,0x1000,0,0x8000001e,0x3f80008,1,&m,put,&irq));assert(!lumio_apply(&u,g,0x1000,0,30,0x3f80008,2,&m,put,&irq));assert(!m.calls);
 puts("linux_uart_model_io: PASS (host-only staged read-clear and IRQ output)");}
