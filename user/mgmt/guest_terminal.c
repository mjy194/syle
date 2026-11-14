#include "guest.h"
#include "guest_terminal.h"
static int64_t call(void*p,uint64_t op,uint64_t h,uint64_t a){(void)p;return syle_guest_call(op,h,a);}
static bool output(void*p,uint8_t b){(void)p;return syle_guest_call(1,(uintptr_t)&b,1)==1;}
static bool say(const char *s){uint64_t n=0;while(s[n])n++;return syle_guest_call(1,(uintptr_t)s,n)==(int64_t)n;}
void mg_terminal_main(void){
    if(!say("\nSYLE.TERMINAL host serial input enabled; Ctrl-] stops guest; wait for shell prompt\n")) {
        syle_guest_call(3,1,0);__builtin_trap();
    }
    const struct mg_continuous_ops ops={0,call,output};
    struct mg_terminal_report report;
    enum mg_terminal_result r=mg_terminal_run(&ops,1u<<19,32,&report);
    bool ok=say(r==MG_TERMINAL_DETACHED?"\nSYLE.TERMINAL detached; guest cleaned up\n":
                r==MG_TERMINAL_BUDGET?"\nSYLE.TERMINAL slice budget reached; guest cleaned up\n":
                "\nSYLE.TERMINAL input, guest, output, or cleanup failed\n");
    syle_guest_call(3,r>=MG_TERMINAL_FAILED||!ok,0);__builtin_trap();
}
