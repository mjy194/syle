/* Exact syscall dispatcher; never execute privileged instructions in tests. */
#include "../ring3.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned input_calls;
#ifdef SYLE_HOST_UART_INPUT
int host_console_recv(void){input_calls++;return 255;}
#endif
void log_text(const char*s){(void)s;}
void log_hex(UINT64 n){(void)n;}
__attribute__((noreturn)) void panic(const char*s){(void)s;abort();}
static int64_t readcall(uint64_t a,uint64_t b){struct frame f={.rax=SYLE_HOST_CONSOLE_RECV,.rdi=a,.rsi=b,.cs=0x2b,.ss=0x33};ring3_syscall(&f);return (int64_t)f.rax;}
int main(void){
#ifdef SYLE_HOST_UART_INPUT
 assert(readcall(0,0)==255&&input_calls==1);
 assert(readcall(1,0)==SYLE_HOST_CONSOLE_INVALID&&input_calls==1);
 assert(readcall(0,1)==SYLE_HOST_CONSOLE_INVALID&&input_calls==1);
#else
 assert(readcall(0,0)==-1&&!input_calls);
 assert(readcall(1,0)==-1&&!input_calls);
#endif
 puts("host input syscall scope/arguments PASS (no hardware I/O)");
}
