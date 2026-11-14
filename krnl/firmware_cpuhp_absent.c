#include "firmware_cpuhp_absent.h"
bool fcha_io(uint16_t port,unsigned width,bool input,uint64_t*rax){
 if(!rax)return false;
 if(port==0xaf00&&width==4){
  if(input){*rax=UINT64_C(0xffffffff);return true;}
  return (uint32_t)*rax==0;
 }
 return port==0xaf05&&width==1&&!input&&(uint8_t)*rax==0;
}
