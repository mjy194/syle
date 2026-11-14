#include "guest.h"
#include <stdbool.h>
void mg_ept_probe_main(void){
 int64_t h=syle_guest_call(SYLE_GUEST_CREATE,0,SYLE_GUEST_BOOT_PROFILE);
 bool pass=false,cleanup=false;
 if(h>0){
  for(unsigned i=0;i<32;i++){
   if(syle_guest_call(i?SYLE_GUEST_STEP:SYLE_GUEST_START,(uint64_t)h,0))break;
   int64_t b=syle_guest_call(SYLE_GUEST_CONSOLE_RECV,(uint64_t)h,0);
   if(b=='P'){pass=true;break;}
   if(b!=-SYLE_GUEST_EBUSY)break;
  }
  int64_t stop=syle_guest_call(SYLE_GUEST_STOP,(uint64_t)h,0);
  int64_t destroy=syle_guest_call(SYLE_GUEST_DESTROY,(uint64_t)h,0);
  cleanup=!stop&&!destroy;
 }else cleanup=syle_guest_call(SYLE_GUEST_REAP,0,0)==0;
 const char*message=pass&&cleanup?"\nSYLE.MGMT managed EPT R/W/X same-RIP retry verified; cleanup passed\n":"\nSYLE.MGMT managed EPT probe failed or cleanup failed\n";
 uint64_t n=0;while(message[n])n++;
 bool printed=syle_guest_call(1,(uintptr_t)message,n)==(int64_t)n;
 syle_guest_call(3,!pass||!cleanup||!printed,0);__builtin_trap();
}
