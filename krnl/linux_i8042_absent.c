#include "linux_i8042_absent.h"
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
bool li8042_read(uint64_t g[15],uint64_t rip,unsigned cpl,uint64_t reason,uint64_t q,unsigned len,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 uint64_t port=q>>16;
 if(!g||!write||cpl||reason!=30||(q&~UINT64_C(0xffff0048))||!(q&8)||(port!=0x60&&port!=0x64)||
 ((q&64)?len!=2:(len!=1||(uint16_t)g[3]!=port))||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len))return false;
 if(!write(ctx,0x681e,rip+len))return false;
 g[0]=(g[0]&~UINT64_C(255))|255;return true;
}
