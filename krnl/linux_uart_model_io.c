#include "linux_uart_model_io.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool lumio_apply(struct linux_uart*u,uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,void*ctx,bool(*write)(void*,uint32_t,uint64_t),bool*irq){
 if(!u||!g||!write||!irq||cpl||reason!=30||len!=1||rip==UINT64_MAX||!canonical(rip)||!canonical(rip+1)||(q&~0xffff0008ULL))return false;
 unsigned port=(unsigned)((q>>16)&0xffff);if(port<0x3f8||port>0x3ff||(uint16_t)g[3]!=port)return false;
 struct linux_uart next=*u;uint8_t value=(uint8_t)g[0];bool out=!(q&8);
 if(!linux_uart_access(&next,port-0x3f8,out,&value))return false;
 bool level=linux_uart_irq_level(&next);
 if(!write(ctx,0x681e,rip+1))return false;
 *u=next;if(!out)g[0]=(g[0]&~0xffULL)|value;
 *irq=level;return true;
}
