#include "guest_wait.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct mock {uint64_t v[0x7000];unsigned reads,writes,rf,wf;};
static bool rd(void*c,uint32_t f,uint64_t*v){struct mock*m=c;if(++m->reads==m->rf)return false;*v=m->v[f];return true;}
static bool wr(void*c,uint32_t f,uint64_t v){struct mock*m=c;if(++m->writes==m->wf)return false;m->v[f]=v;return true;}
static void init(struct mock*m){memset(m,0,sizeof *m);m->v[0x4402]=12;m->v[0x440c]=1;m->v[0x681e]=0xffffffff81000000ull;m->v[0x0802]=16;m->v[0x4816]=0xa09b;m->v[0x4002]=0x80000080;m->v[0x401e]=0x42;m->v[0x6800]=0x80050033;m->v[0x6804]=0x20b0;m->v[0x2806]=0xd01;m->v[0x4012]=0x200;}
int main(void){static struct mock m;for(unsigned b=0;b<3;b++){init(&m);m.v[0x4824]=b;assert(gw_halt(&m,rd,wr));assert(m.v[0x4824]==0&&m.v[0x4826]==1&&m.v[0x681e]==0xffffffff81000001ull&&m.writes==3);}
for(unsigned i=1;i<=15;i++){init(&m);m.rf=i;assert(!gw_halt(&m,rd,wr)&&!m.writes);}for(unsigned i=1;i<=3;i++){init(&m);m.wf=i;assert(!gw_halt(&m,rd,wr)&&m.writes==i);}
const unsigned fields[]={0x4402,0x440c,0x0802,0x4816,0x4826,0x4824,0x4002,0x401e,0x6800,0x6804,0x2806,0x4012};for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){init(&m);m.v[fields[i]]=UINT64_MAX;assert(!gw_halt(&m,rd,wr)&&!m.writes);}
uint64_t bad[]={UINT64_MAX,0x00007fffffffffffull,0x800000000000ull};for(unsigned i=0;i<3;i++){init(&m);m.v[0x681e]=bad[i];assert(!gw_halt(&m,rd,wr)&&!m.writes);}
init(&m);assert(!gw_window(&m,rd,wr,0,0,true)&&!m.writes);assert(gw_window(&m,rd,wr,0,4,true)&&m.v[0x4002]==0x80000084);assert(gw_window(&m,rd,wr,0,4,true)&&m.writes==1);assert(gw_window(&m,rd,wr,0,4,false)&&m.v[0x4002]==0x80000080);
init(&m);m.v[0x4402]=7;m.v[0x4002]|=4;m.v[0x6820]=0x202;uint64_t rip=m.v[0x681e];assert(gw_window_exit(&m,rd,wr,0,4)&&m.v[0x681e]==rip&&m.v[0x4002]==0x80000080);
for(unsigned i=1;i<=6;i++){init(&m);m.rf=i;assert(!gw_window_exit(&m,rd,wr,0,4)&&!m.writes);}init(&m);m.v[0x4824]=3;assert(!gw_halt(&m,rd,wr));init(&m);m.v[0x6820]=0x100;assert(!gw_halt(&m,rd,wr));init(&m);m.v[0x6822]=1;assert(!gw_halt(&m,rd,wr));assert(!gw_window(&m,rd,wr,4,4,false));puts("guest wait mock PASS; not runtime");}
