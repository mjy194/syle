#include "linux_uart_io.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool luart_apply(struct guest_uart*u,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,void*ctx,bool(*write)(void*,uint32_t,uint64_t),bool*emit,uint8_t*byte){
 if(!u||!g||!write||!emit||!byte||cpl||reason!=30||len!=1||rip==UINT64_MAX||!canonical(rip)||!canonical(rip+1)||(q&~0xffff0008ULL))return false;
 unsigned port=(unsigned)((q>>16)&0xffff);if(port<0x3f8||port>0x3ff||(uint16_t)g[3]!=port)return false;
 struct guest_uart staged=*u;uint32_t value=(uint8_t)g[0];bool sent;uint8_t ch;
 bool out=!(q&8);
 if(!guest_uart_access(&staged,port-0x3f8,1,out,&value,&sent,&ch))return false;
 if(!write(ctx,0x681e,rip+1))return false;
 *u=staged;if(!out)g[0]=(g[0]&~0xffULL)|(uint8_t)value;
 *emit=sent;*byte=ch;return true;
}
