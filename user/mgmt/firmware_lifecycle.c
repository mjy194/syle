#include "guest.h"
#include "firmware_lifecycle.h"
static int64_t call(void*x,uint64_t op,uint64_t h,uint64_t arg){(void)x;return syle_guest_call(op,h,arg);}
static bool output(void*x,uint8_t b){(void)x;return syle_guest_call(1,(uintptr_t)&b,1)==1;}
void firmware_lifecycle_main(void){
 const struct mg_continuous_ops ops={0,call,output};int failed=fml_run(&ops);
 const char*message=failed?"\nSYLE.MGMT FIRMWARE_LIFECYCLE_FAILED\n":"\nSYLE.MGMT FIRMWARE_LIFECYCLE_PASS\n";
 uint64_t n=0;while(message[n])n++;if(syle_guest_call(1,(uintptr_t)message,n)!=(int64_t)n)failed=1;
 syle_guest_call(3,failed,0);__builtin_trap();
}
