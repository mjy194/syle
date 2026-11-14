#include "../../krnl/guest_control.h"
#include "../../krnl/ring3.h"
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static jmp_buf stopped;
static unsigned releases,stops,fatals;
static bool owned;
struct frame {uint64_t r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rdx,rcx,rbx,rax;uint64_t vector,error,rip,cs,rflags,rsp,ss;};
extern void ring3_syscall(struct frame *);
extern void ring3_fault(const struct frame *,uint64_t);
extern void exception_panic(const struct frame *);
void exception_stop(void){fatals++;longjmp(stopped,1);}
void log_output_mode(unsigned m){(void)m;}void console_panic_style(void){}
void log_text(const char *s){(void)s;}void log_hex(uint64_t n){(void)n;}
static int prepare(void *c,uint64_t profile){(void)c;assert(profile==1);owned=true;return 0;}
static int start(void *c){(void)c;return 0;}
static int stop(void *c){(void)c;stops++;return 0;}
static int release(void *c){(void)c;releases++;owned=false;return 0;}
int main(int argc,char **argv){assert(argc==5||argc==6);bool running=argc==6;unsigned vector=(unsigned)strtoul(argv[1],0,0),cpl=(unsigned)strtoul(argv[2],0,0),expected=(unsigned)strtoul(argv[3],0,0),direct=(unsigned)strtoul(argv[4],0,0);
 struct guest_backend backend={prepare,start,stop,release,0,0,0};assert(ring3_guest_bind(&backend,0));
 struct frame call={.cs=0x2b,.ss=0x33,.rax=SYLE_GUEST_CREATE,.rsi=1};ring3_syscall(&call);assert(call.rax==1);
 if(running){call.rax=SYLE_GUEST_START;call.rdi=1;call.rsi=0;ring3_syscall(&call);assert(!call.rax);}
 struct frame f={.vector=vector,.cs=cpl==3?0x2b:8};
 if(direct){if(!setjmp(stopped))ring3_fault(&f,0);}else{if(!setjmp(stopped)){exception_panic(&f);assert(!"fatal returned");}}
 assert(releases==expected&&stops==(running?expected:0)&&owned==!expected);
 if(!direct)assert(fatals==1);
 printf("actual exception/ring3/controller host path: vector=%u cpl=%u cleanup=%u direct=%u PASS\n",vector,cpl,releases,direct);return 0;
}
