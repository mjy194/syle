#define SYLE_DIAG_HOST_TEST
#define SYLE_HOST_UART_INPUT
#include "../runtime.c"
#include <assert.h>
#include <stdio.h>
static uint8_t status,byte;static unsigned reads,data_reads;
uint8_t diag_test_in8(uint16_t port){reads++;if(port==0x3fd)return status;assert(port==0x3f8);data_reads++;return byte;}
void diag_test_out8(uint16_t port,uint8_t b){(void)port;(void)b;assert(0);}
int main(void){
 uart_present=0;assert(host_console_recv()==-2&&!reads);
 uart_present=1;status=0x60;assert(host_console_recv()==-1&&reads==1&&!data_reads);
 status=0x61;byte=0x1d;assert(host_console_recv()==0x1d&&data_reads==1);
 status=0x63;assert(host_console_recv()==-3&&data_reads==1);
 status=0xff;assert(host_console_recv()==-2&&data_reads==1);
 puts("host UART exact runtime integration PASS (mock registers)");
}
