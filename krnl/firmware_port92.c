#include "firmware_port92.h"
bool fp92_io(uint16_t port,unsigned width,bool input,uint64_t*rax){
 if(!rax||port!=0x92||width!=1)return false;
 if(!input)return (uint8_t)*rax==2;
 *rax=(*rax&~UINT64_C(255))|2;return true;
}
