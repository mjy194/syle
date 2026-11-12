#include "../linux_gs_commit.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct mock{unsigned n,fail;uint64_t f[2],v[2];};
static bool put(void*p,uint32_t f,uint64_t v){struct mock*m=p;assert(m->n<2);m->f[m->n]=f;m->v[m->n]=v;return ++m->n!=m->fail;}
int main(void){
 uint64_t g[15]={0},old[15];struct mock m={0};
 g[0]=0x1637000;g[2]=0xc0000101;g[3]=0;
 assert(lgsc_apply(g,0x100001c,0,0,32,2,&m,put));
 assert(m.n==2&&m.f[0]==0x6810&&m.v[0]==0x1637000&&m.f[1]==0x681e&&m.v[1]==0x100001e);
 for(unsigned fail=1;fail<=2;fail++){m=(struct mock){.fail=fail};memcpy(old,g,sizeof g);assert(!lgsc_apply(g,0x100001c,0,0,32,2,&m,put));assert(m.n==fail&&!memcmp(g,old,sizeof g));}
 m=(struct mock){0};assert(lgsc_apply(g,0x100001c,0xffff800012345678ULL,0,31,2,&m,put));assert(m.n==1&&m.f[0]==0x681e&&g[0]==0x12345678&&g[3]==0xffff8000);
 m=(struct mock){.fail=1};memcpy(old,g,sizeof g);assert(!lgsc_apply(g,0x100001c,0,0,31,2,&m,put));assert(!memcmp(g,old,sizeof g));
 #define REJECT(reason,len,cpl) do{m=(struct mock){0};memcpy(old,g,sizeof g);assert(!lgsc_apply(g,0x100001c,0,cpl,reason,len,&m,put));assert(!m.n&&!memcmp(g,old,sizeof g));}while(0)
 REJECT(32,1,0);REJECT(32,2,3);REJECT(10,2,0);REJECT(0x80000020,2,0);
 g[2]=0xc0000102;REJECT(32,2,0);g[2]=0xc0000101;
 g[0]=0;g[3]=0x8000;REJECT(32,2,0);
 puts("linux_gs_commit: PASS");
}
