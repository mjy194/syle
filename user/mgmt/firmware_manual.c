#include "guest.h"
#include "../../include/syle/host_console.h"
#include <stdbool.h>

static void print(const char *s) {
    unsigned n=0;
    while(s[n]) n++;
    syle_guest_call(1,(uintptr_t)s,n);
}

void firmware_lifecycle_main(void) {
    int64_t handle=0;
    bool running=false, attached=false, failed=false;
    const char *menu="\nM2 [c]reate "
#ifdef SYLE_M2_NESTED
        "[n]ested create "
#endif
        "[s]tart/resume [p]ause [a]ttach [d]estroy [q]uit\n";
    print(menu);
    for (;;) {
        if (running) {
            if (syle_guest_call(SYLE_GUEST_STEP,handle,0)) { failed=true; break; }
            for (unsigned i=0;i<128;i++) {
                int64_t byte=syle_guest_call(SYLE_GUEST_CONSOLE_RECV,handle,0);
                if (byte==-SYLE_GUEST_EBUSY) break;
                if (byte<0 || byte>255 || syle_guest_call(1,(uintptr_t)&byte,1)!=1) { failed=true; break; }
            }
            if (failed) break;
            if (attached) {
                int64_t space=syle_guest_call(SYLE_GUEST_CONSOLE_SPACE,handle,0);
                if (space<0) {failed=true; break;}
                if (!space) continue;
            }
        }
        int64_t key=syle_guest_call(SYLE_HOST_CONSOLE_RECV,0,0);
        if (key==SYLE_HOST_CONSOLE_EMPTY) continue;
        if (key<0 || key>255) { failed=true; break; }
        if (attached && key!=29) {
            if (syle_guest_call(SYLE_GUEST_CONSOLE_SEND,handle,key)) {failed=true;break;}
        } else if (attached) {
            attached=false;
            print(menu);
        } else if (!handle && (key=='c'
#ifdef SYLE_M2_NESTED
                               || key=='n'
#endif
                               )) {
            unsigned profile=key=='c'?SYLE_GUEST_BOOT_PROFILE:SYLE_GUEST_NESTED_BOOT_PROFILE;
            handle=syle_guest_call(SYLE_GUEST_CREATE,0,profile);
            if (handle<=0) {syle_guest_call(SYLE_GUEST_REAP,0,0);handle=0;failed=true;break;}
            print("M2 ready\n");
        } else if (key=='s' && handle && !running) {
            if (syle_guest_call(SYLE_GUEST_START,handle,0)) {failed=true;break;}
            running=true;
            print("M2 running\n");
        } else if (key=='p' && running) {
            if (syle_guest_call(SYLE_GUEST_STOP,handle,0)) {failed=true;break;}
            running=false;
            print("M2 paused\n");
        } else if (key=='a' && running) {
            attached=true;
            print("M2 attached (Ctrl-] for management)\n");
        } else if (key=='d' && handle) {
            if (syle_guest_call(SYLE_GUEST_DESTROY,handle,0)) {failed=true;break;}
            running=false;handle=0;
            print("M2 destroyed\n");
        } else if (key=='q') break;
        else if (key!='\r' && key!='\n') print(menu);
    }
    if (handle && syle_guest_call(SYLE_GUEST_DESTROY,handle,0)) failed=true;
    print(failed?"M2 failed\n":"M2 closed\n");
    syle_guest_call(3,failed,0);
    __builtin_trap();
}
