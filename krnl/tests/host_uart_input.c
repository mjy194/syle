#include "../host_uart_input.h"
#include <assert.h>
#include <stdio.h>
struct fake {unsigned status,byte,lsr_reads,data_reads;};
static uint8_t read8(void *p,uint16_t port){struct fake*f=p;if(port==0x3fd){f->lsr_reads++;return f->status;}assert(port==0x3f8);f->data_reads++;return f->byte;}
int main(void){
 struct fake f={.status=1,.byte=255};
 assert(host_uart_poll(false,&f,read8)==-2&&!f.lsr_reads);
 assert(host_uart_poll(true,&f,0)==-2&&!f.lsr_reads);
 assert(host_uart_poll(true,&f,read8)==255&&f.data_reads==1);
 f.status=0x60;assert(host_uart_poll(true,&f,read8)==-1&&f.data_reads==1);
 for(unsigned error=2;error<=16;error*=2){f.status=error|1;assert(host_uart_poll(true,&f,read8)==-3&&f.data_reads==1);}
 f.status=255;assert(host_uart_poll(true,&f,read8)==-2&&f.data_reads==1);
 puts("host UART nonblocking read policy PASS (mock I/O only)");
}
