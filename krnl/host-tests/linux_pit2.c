#include "../linux_pit2.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned writes;static bool fail;static uint64_t rip;
static bool wr(void*c,uint32_t f,uint64_t v){(void)c;assert(f==0x681e);writes++;if(fail)return false;rip=v;return true;}
static uint64_t tick(uint64_t t){return t;}
static void access(struct lpit2*p,unsigned port,bool out,uint8_t*v,uint64_t ns){assert(lpit2_access(p,port,out,v,ns));}
static void program(struct lpit2*p,unsigned count){uint8_t v=1;access(p,0x61,true,&v,0);v=0xb0;access(p,0x43,true,&v,0);v=(uint8_t)count;access(p,0x42,true,&v,0);v=(uint8_t)(count>>8);access(p,0x42,true,&v,0);}
int main(void){struct lpit2 p;uint8_t v;assert(lpit2_init(&p,0));program(&p,3);
 v=0;access(&p,0x42,false,&v,tick(1));assert(v==3);access(&p,0x42,false,&v,tick(2));assert(v==0&&p.count==2); /* Live, not latched pair. */
 v=0;access(&p,0x61,true,&v,tick(2));access(&p,0x61,false,&v,tick(20));assert(p.count==2&&!p.out);
 v=3;access(&p,0x61,true,&v,tick(20));access(&p,0x61,false,&v,tick(21));assert(p.count==1&&v==3);
 access(&p,0x61,false,&v,tick(22));assert(p.out&&p.count==0&&v==0x23);v=0x21;access(&p,0x61,true,&v,tick(23));assert(p.out&&p.gate&&!p.speaker&&p.count==65535);
 assert(lpit2_init(&p,0));program(&p,0);access(&p,0x61,false,&v,tick(1));assert(p.count==65536&&!p.out);access(&p,0x61,false,&v,tick(65536));assert(p.count==1&&!p.out);access(&p,0x61,false,&v,tick(65537));assert(p.out);
 access(&p,0x61,false,&v,tick(65537+2*65536+5));assert(p.out&&p.count==65531);
 v=0;access(&p,0x61,true,&v,p.ticks);uint64_t paused=p.ticks;access(&p,0x61,false,&v,paused+100000);assert(p.out&&p.count==65531);
 v=0x34;access(&p,0x42,true,&v,p.ticks);assert(!p.out&&p.waiting&&p.write_high);
 uint32_t held=p.count;access(&p,0x61,false,&v,p.ticks+10000);assert(p.count==held&&!p.out);
 v=0x12;access(&p,0x42,true,&v,p.ticks);assert(p.loading&&p.reload==0x1234);
 access(&p,0x61,false,&v,p.ticks+1);assert(!p.loading&&p.count==0x1234&&!p.gate);
 struct lpit2 old=p;v=0xb6;assert(!lpit2_access(&p,0x43,true,&v,p.ticks)&&!memcmp(&p,&old,sizeof p));v=4;assert(!lpit2_access(&p,0x61,true,&v,p.ticks));assert(!lpit2_access(&p,0x40,false,&v,p.ticks));assert(!lpit2_access(&p,0x61,false,&v,p.ticks-1));
 uint64_t g[15]={0};assert(lpit2_init(&p,0));g[0]=0xabcdef0000000001ULL;assert(lpit2_io(&p,g,0x1000,0,30,(0x61ull<<16)|64,2,0,0,wr)==LPIT_COMMITTED&&rip==0x1002&&p.gate);
 assert(lpit2_io(&p,g,0x1000,0,30,(0x61ull<<16)|72,2,tick(10),0,wr)==LPIT_COMMITTED&&g[0]==0xabcdef0000000001ULL);
 g[3]=0x61;assert(lpit2_io(&p,g,0x1000,0,30,(0x61ull<<16)|8,1,tick(11),0,wr)==LPIT_COMMITTED);
 for(unsigned bit=0;bit<16;bit++){if(bit==3||bit==6)continue;assert(lpit2_io(&p,g,0x1000,0,30,(0x61ull<<16)|72|(1u<<bit),2,p.ticks,0,wr)==LPIT_REJECTED);}
 old=p;uint64_t rax=g[0];fail=true;assert(lpit2_io(&p,g,0x1000,0,30,(0x61ull<<16)|72,2,tick(12),0,wr)==LPIT_FATAL&&p.failed&&p.ticks==old.ticks&&g[0]==rax);fail=false;
 assert(lpit2_init(&p,0));program(&p,1000);g[0]=g[3]=UINT64_MAX;
 assert(lpit2_rdtsc(&p,g,0x2000,0,16,2,100,0,wr)==LPIT_COMMITTED&&g[0]==100000&&!g[3]&&p.count==901);
 assert(lpit2_rdtsc(&p,g,0x2000,0,16,2,UINT64_MAX/1000,0,wr)==LPIT_COMMITTED&&(((g[3]<<32)|g[0])==(UINT64_MAX/1000)*1000)&&p.out);
 assert(lpit2_rdtsc(&p,g,0x2000,0,16,2,UINT64_MAX/1000+1,0,wr)==LPIT_REJECTED);
 puts("linux_pit2: mode0 gate/load/live reads/output, immediate+DX staged IO, common exact tick/PIT/TSC and failure atomicity PASS (host only)");return 0;}
