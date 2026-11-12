#include "../linux_uart_runtime.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct sink{unsigned calls;bool fail,level,reenter;struct linux_uart_runtime*c;};
static bool irq(void*p,unsigned pin,bool level){struct sink*s=p;assert(pin==4);s->calls++;s->level=level;if(s->reenter)assert(lurt_tick(s->c,1)==LURT_REJECTED);return !s->fail;}
static bool write(void*p,uint32_t f,uint64_t v){assert(f==0x681e&&v==0x1001);return !*(bool*)p;}
static void wr(struct linux_uart_runtime*c,unsigned reg,uint8_t v){assert(linux_uart_access(&c->uart,reg,true,&v));}
int main(void){struct linux_uart_runtime c;struct sink s={.fail=true,.c=&c,.reenter=true};assert(lurt_init(&c,&s,irq));assert(c.irq_dirty);
 wr(&c,3,3);wr(&c,4,8);wr(&c,1,1);bool accepted;
 assert(lurt_receive(&c,65,&accepted)==LURT_IRQ_PENDING&&accepted&&c.uart.rc==1&&c.irq_dirty&&s.level);
 struct linux_uart old=c.uart;s.fail=false;assert(lurt_retry_irq(&c)==LURT_COMMITTED&&!c.irq_dirty&&!memcmp(&old,&c.uart,sizeof old));
 assert(lurt_receive(&c,66,&accepted)==LURT_COMMITTED&&!accepted&&c.uart.overrun&&c.uart.rc==1);
 uint64_t g[15]={0};g[3]=0x3f8;bool fail=false;s.fail=true;
 assert(lurt_guest_io(&c,g,0x1000,0,30,0x3f80008,1,&fail,write)==LURT_IRQ_PENDING&&g[0]==65&&c.uart.rc==0);
 old=c.uart;s.fail=false;assert(lurt_retry_irq(&c)==LURT_COMMITTED&&!memcmp(&old,&c.uart,sizeof old)&&!s.level);
 g[0]=77;fail=true;unsigned calls=s.calls;old=c.uart;
 assert(lurt_guest_io(&c,g,0x1000,0,30,0x3f80000,1,&fail,write)==LURT_REJECTED&&s.calls==calls&&!memcmp(&old,&c.uart,sizeof old));
 fail=false;assert(lurt_guest_io(&c,g,0x1000,0,30,0x3f80000,1,&fail,write)==LURT_COMMITTED);
 bool available;uint8_t byte;s.fail=true;
 assert(lurt_pop_tx(&c,&available,&byte)==LURT_IRQ_PENDING&&available&&byte==77&&c.uart.tc==0);
 s.fail=false;assert(lurt_retry_irq(&c)==LURT_COMMITTED);assert(lurt_pop_tx(&c,&available,&byte)==LURT_COMMITTED&&!available&&byte==0);
 s.fail=true;assert(lurt_modem(&c,0xb0)==LURT_IRQ_PENDING&&c.uart.modem==0xb0);old=c.uart;s.fail=false;assert(lurt_retry_irq(&c)==LURT_COMMITTED&&!memcmp(&old,&c.uart,sizeof old));
 assert(lurt_receive(&c,88,&accepted)==LURT_COMMITTED&&accepted);s.fail=true;assert(lurt_tick(&c,4)==LURT_IRQ_PENDING&&c.uart.rx_idle==4);old=c.uart;s.fail=false;assert(lurt_retry_irq(&c)==LURT_COMMITTED&&!memcmp(&old,&c.uart,sizeof old));
 old=c.uart;calls=s.calls;assert(lurt_modem(&c,1)==LURT_REJECTED&&s.calls==calls&&!memcmp(&old,&c.uart,sizeof old));
 puts("linux_uart_runtime: PASS (host-only IRQ sync bridge)");}
