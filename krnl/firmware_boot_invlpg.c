#include "firmware_boot_invlpg.h"
bool fbinv_decode(const uint8_t*b,size_t n,unsigned len,const uint64_t regs[16],uint64_t linear){
 if(!b||!regs||n<len||((linear>>47)!=0&&(linear>>47)!=0x1ffff))return false;
 unsigned at=0,rex=0;
 if(len==4){rex=b[at++];if((rex&0xf0)!=0x40||(rex&6))return false;}else if(len!=3)return false;
 if(b[at++]!=0x0f||b[at++]!=1)return false;
 uint8_t mod=b[at];unsigned base=(mod&7)+((rex&1)?8:0);
 if((mod&0xf8)!=0x38||(mod&7)==4||(mod&7)==5)return false;
 return regs[base]==linear;
}
