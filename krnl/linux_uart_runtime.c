#include "linux_uart_runtime.h"
static bool begin(struct linux_uart_runtime*c){if(!c||!c->ready||!c->sync_irq||c->busy)return false;c->busy=true;return true;}
static enum lurt_result reject(struct linux_uart_runtime*c){c->busy=false;return LURT_REJECTED;}
static enum lurt_result sync(struct linux_uart_runtime*c){
 c->irq_dirty=true;
 bool ok=c->sync_irq(c->irq_context,4,linux_uart_irq_level(&c->uart));
 c->irq_dirty=!ok;c->busy=false;return ok?LURT_COMMITTED:LURT_IRQ_PENDING;
}
bool lurt_init(struct linux_uart_runtime*c,void*x,bool(*fn)(void*,unsigned,bool)){
 if(!c||!fn)return false;
 *c=(struct linux_uart_runtime){.irq_context=x,.sync_irq=fn,.irq_dirty=true,.ready=true};linux_uart_init(&c->uart);return true;
}
enum lurt_result lurt_retry_irq(struct linux_uart_runtime*c){if(!begin(c))return LURT_REJECTED;return sync(c);}
enum lurt_result lurt_receive(struct linux_uart_runtime*c,uint8_t b,bool*accepted){
 if(!accepted||!begin(c))return LURT_REJECTED;
 if(!linux_uart_receive(&c->uart,b,accepted))return reject(c);
 return sync(c);
}
enum lurt_result lurt_tick(struct linux_uart_runtime*c,unsigned n){
 if(!begin(c))return LURT_REJECTED;
 if(!linux_uart_tick(&c->uart,n))return reject(c);
 return sync(c);
}
enum lurt_result lurt_modem(struct linux_uart_runtime*c,uint8_t b){
 if(!begin(c))return LURT_REJECTED;
 if(!linux_uart_modem_input(&c->uart,b))return reject(c);
 return sync(c);
}
enum lurt_result lurt_pop_tx(struct linux_uart_runtime*c,bool*available,uint8_t*byte){
 if(!available||!byte||!begin(c))return LURT_REJECTED;
 struct linux_uart next=c->uart;uint8_t value=0;
 if(!linux_uart_access(&next,3,false,&value))return reject(c);
 value=0;bool have=next.tc!=0;
 if(have&&!linux_uart_drain(&next,&value))return reject(c);
 c->uart=next;*available=have;*byte=value;return sync(c);
}
enum lurt_result lurt_guest_io(struct linux_uart_runtime*c,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,void*x,bool(*write)(void*,uint32_t,uint64_t)){
 if(!begin(c))return LURT_REJECTED;
 bool irq;
 if(!lumio_apply(&c->uart,g,rip,cpl,reason,q,len,x,write,&irq))return reject(c);
 return sync(c);
}
