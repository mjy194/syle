#include "../linux_uart_io.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct mock{unsigned calls;bool fail;};
static bool put(void*p,uint32_t f,uint64_t v){struct mock*m=p;assert(f==0x681e&&v==0x1001);m->calls++;return !m->fail;}
int main(void){struct guest_uart u={.lcr=3},saved;uint64_t g[15]={0},old[15];g[0]='A';g[3]=0x3f8;bool emit=false;uint8_t byte=7;struct mock m={0};
 assert(luart_apply(&u,g,0x1000,0,30,0x3f80000,1,&m,put,&emit,&byte));assert(emit&&byte=='A'&&m.calls==1);
 saved=u;memcpy(old,g,sizeof g);m=(struct mock){.fail=true};emit=false;byte=0x55;
 assert(!luart_apply(&u,g,0x1000,0,30,0x3f80000,1,&m,put,&emit,&byte));
 assert(m.calls==1&&!memcmp(&u,&saved,sizeof u)&&!memcmp(g,old,sizeof g)&&!emit&&byte==0x55);

 g[0]=0x1122334455667788ULL;g[3]=0x3fd;m=(struct mock){0};assert(luart_apply(&u,g,0x1000,0,30,0x3fd0008,1,&m,put,&emit,&byte));assert(g[0]==0x1122334455667760ULL&&!emit&&byte==0);
 g[3]=0x3fb;g[0]=0x83;saved=u;memcpy(old,g,sizeof g);m=(struct mock){.fail=true};emit=true;byte=99;
 assert(!luart_apply(&u,g,0x1000,0,30,0x3fb0000,1,&m,put,&emit,&byte));assert(!memcmp(&u,&saved,sizeof u)&&!memcmp(g,old,sizeof g)&&emit&&byte==99);
 #define BAD(q,len,cpl,reason) do{m=(struct mock){0};assert(!luart_apply(&u,g,0x1000,cpl,reason,q,len,&m,put,&emit,&byte));assert(!m.calls&&!memcmp(&u,&saved,sizeof u)&&!memcmp(g,old,sizeof g));}while(0)
 BAD(0x3fb0010,1,0,30);BAD(0x3fb0020,1,0,30);BAD(0x3fb0040,1,0,30);BAD(0x3fb0001,1,0,30);BAD(0x3fb0000,2,0,30);BAD(0x3fb0000,1,3,30);BAD(0x3fb0000,1,0,0x8000001e);BAD(0x3f80000,1,0,30);BAD(0x800003fb0000ULL,1,0,30);
 m=(struct mock){0};assert(luart_apply(&u,g,0x1000,0,30,0x3fb0000,1,&m,put,&emit,&byte));assert(u.lcr==0x83);
 g[3]=0x3f8;g[0]=1;m=(struct mock){0};assert(luart_apply(&u,g,0x1000,0,30,0x3f80000,1,&m,put,&emit,&byte));assert(u.dll==1&&!emit);
 puts("linux_uart_io: PASS (host only, TX polling subset)");}
