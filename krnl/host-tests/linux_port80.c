#include "linux_port80.h"
#include <assert.h>
#include <stdio.h>
struct mock{unsigned calls;bool fail;uint64_t rip;};
static bool wr(void*p,uint32_t f,uint64_t v){struct mock*m=p;assert(f==0x681e);m->calls++;m->rip=v;return !m->fail;}
int main(void){uint64_t g[15]={0};struct lp80_state s={0};struct mock m={0};g[0]=0x1234ff;g[3]=0x80;assert(lp80_io(&s,g,0x1000,0,30,0x800040,2,&m,wr));assert(s.last==255&&s.writes==1&&m.rip==0x1002&&g[0]==0x1234ff);
 g[0]=0x42;assert(lp80_io(&s,g,0x1000,0,30,0x800000,1,&m,wr)&&s.last==0x42&&s.writes==2);
 for(unsigned i=0;i<10;i++){s=(struct lp80_state){0};m=(struct mock){0};uint64_t q=0x800040,rip=0x1000;unsigned len=2,cpl=0;if(i<5)q|=1ull<<i;if(i==5)len=1;if(i==6)cpl=3;if(i==7)rip=UINT64_MAX;if(i==8)rip=0x800000000000ull;if(i==9)q=0x810040;assert(!lp80_io(&s,g,rip,cpl,30,q,len,&m,wr)&&s.failed&&!s.writes&&!m.calls);}
 s=(struct lp80_state){.writes=4,.last=7};m=(struct mock){.fail=true};assert(!lp80_io(&s,g,0x1000,0,30,0x800040,2,&m,wr)&&s.writes==4&&s.last==7&&s.failed);assert(!lp80_io(&s,g,0x1000,0,30,0x800040,2,&m,wr)&&m.calls==1);
 s=(struct lp80_state){.writes=UINT64_MAX};m=(struct mock){0};assert(!lp80_io(&s,g,0x1000,0,30,0x800040,2,&m,wr)&&!m.calls);
 s=(struct lp80_state){0};g[3]=0x81;assert(!lp80_io(&s,g,0x1000,0,30,0x800000,1,&m,wr));puts("port80 model PASS");}
