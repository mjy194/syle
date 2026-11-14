#include "guest.h"
#include "guest_continuous.h"
static int64_t call(void *p,uint64_t op,uint64_t h,uint64_t a) {
    (void)p;return syle_guest_call(op,h,a);
}
static bool output(void *p,uint8_t b) {
    (void)p;return syle_guest_call(1,(uintptr_t)&b,1)==1;
}
void mg_continuous_main(void) {
    const struct mg_continuous_ops ops={0,call,output};
    enum mg_continuous_result r=mg_continuous_run(&ops,1u<<18,32);
    static const char bounded[]="\nSYLE.MGMT continuous budget reached; not Linux shell acceptance\n";
    static const char failed[]="\nSYLE.MGMT continuous execution or cleanup failed\n";
    const char *msg=r==MG_BUDGET_REACHED?bounded:failed;
    uint64_t len=r==MG_BUDGET_REACHED?sizeof(bounded)-1:sizeof(failed)-1;
    bool printed=syle_guest_call(1,(uintptr_t)msg,len)==(int64_t)len;
    syle_guest_call(3,r!=MG_BUDGET_REACHED||!printed,0);
    __builtin_trap();
}
