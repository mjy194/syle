#include "pic0_ports.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct fixture{struct pic0 m;struct pcp s;uint64_t g[15];unsigned writes;bool fail,reenter;};
static bool wr(void*c,uint32_t f,uint64_t v){struct fixture*x=c;assert(f==0x681e&&v==0x1002);x->writes++;if(x->reenter){assert(!pcp_entry_ok(&x->s,0));assert(!pcp_io(&x->s,x->g,0x1000,0,30,0x210040,2,0,x,wr));}return !x->fail;}
static void init(struct fixture*x){memset(x,0,sizeof(*x));assert(pic0_init(&x->m,1,0));assert(pcp_init(&x->s,&x->m));}
static bool io(struct fixture*x,unsigned p,bool in,unsigned b,unsigned flags){x->g[0]=0xabc000|b;return pcp_io(&x->s,x->g,0x1000,0,30,((uint64_t)p<<16)|64|(in?8:0),2,flags,x,wr);}
static void master(struct fixture*x){assert(io(x,0x20,0,0x11,0));assert(io(x,0x21,0,0x30,0));assert(io(x,0x21,0,4,0));assert(io(x,0x21,0,1,0));assert(io(x,0x21,0,0xfe,0));}
int main(void){struct fixture x;init(&x);assert(io(&x,0x21,0,251,512));assert(io(&x,0x21,1,0,512)&&x.g[0]==0xabc0fb);uint8_t cold;assert(!pic0_peek(&x.m,&cold));assert(io(&x,0x21,0,255,512));assert(!io(&x,0x21,0,254,0));init(&x);assert(!io(&x,0x21,0,250,0));init(&x);master(&x);assert(pcp_entry_ok(&x.s,512));assert(io(&x,0x21,1,0,512)&&x.g[0]==0xabc0fe);
 assert(io(&x,0xa0,0,0x11,0));assert(!pcp_entry_ok(&x.s,512));assert(io(&x,0xa1,0,0x38,0));assert(io(&x,0xa1,0,2,0));assert(io(&x,0xa1,0,1,0));assert(io(&x,0xa1,0,255,0));assert(io(&x,0x21,0,250,512));assert(x.s.slave.master==255);
 assert(pic0_edges(&x.m,1,1));uint64_t t;uint8_t v;assert(pic0_reserve(&x.m,&t,&v));unsigned w=x.writes;assert(!io(&x,0xa1,0,255,0)&&x.writes==w&&x.m.reserved==t);
 init(&x);assert(!io(&x,0x20,0,17,512)&&!x.writes);init(&x);master(&x);assert(io(&x,0x21,0,250,0));assert(!io(&x,0xa0,0,17,0)&&x.s.slave.slave==255);
 init(&x);x.fail=true;struct pic0 old=x.m;assert(!io(&x,0x20,0,17,0));assert(!memcmp(&old,&x.m,sizeof old)&&x.s.failed);assert(!io(&x,0x21,0,255,0)&&x.writes==1);
 init(&x);x.fail=true;assert(!io(&x,0xa0,0,17,0)&&x.s.slave.slave==255&&!x.s.slave.chip[1].started);
 init(&x);x.fail=true;assert(!io(&x,0xa1,1,0x55,0)&&x.g[0]==0xabc055);
 init(&x);x.reenter=true;assert(io(&x,0xa1,0,255,0));
 for(unsigned k=0;k<6;k++){init(&x);uint64_t q=0x210040,rip=0x1000;unsigned cpl=0,len=2; if(k==0)q|=16;if(k==1)q|=32;if(k==2)q|=1;if(k==3)cpl=3;if(k==4)len=1;if(k==5)rip=UINT64_C(0x800000000000);assert(!pcp_io(&x.s,x.g,rip,cpl,30,q,len,0,&x,wr)&&!x.writes);}
 puts("pic0 ports PASS");}
