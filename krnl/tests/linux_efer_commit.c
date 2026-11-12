#include "../linux_efer_commit.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct mock{unsigned n,fail;uint32_t fields[3];uint64_t values[3];};
static bool put(void*p,uint32_t f,uint64_t v){struct mock*m=p;assert(m->n<3);m->fields[m->n]=f;m->values[m->n]=v;return ++m->n!=m->fail;}
static bool inv(void*p,uint64_t e){return put(p,0xffff,e);}
int main(void){
 struct lefer_env e={0x500,0x80000033,0x20,0x200,0x12301,(1u<<11)|(1u<<20)};
 uint64_t g[15]={0},old[15];g[2]=0xc0000080;struct mock m={0};
 assert(lefer_apply(g,0x1000,0,31,2,&e,&m,put,inv));assert(g[0]==0x500&&g[3]==0&&m.n==1&&m.fields[0]==0x681e);
 g[0]=0xd01; /* Retain both long-mode bits; only SCE/NXE change. */
 m=(struct mock){0};assert(lefer_apply(g,0x1000,0,32,2,&e,&m,put,inv));assert(m.n==3&&m.fields[0]==0x2806&&m.values[0]==0xd01&&m.fields[1]==0xffff&&m.values[1]==e.eptp&&m.fields[2]==0x681e);
 for(unsigned fail=1;fail<=3;fail++){m=(struct mock){.fail=fail};memcpy(old,g,sizeof g);assert(!lefer_apply(g,0x1000,0,32,2,&e,&m,put,inv));assert(m.n==fail&&!memcmp(g,old,sizeof g));}
 g[0]=0x501;m=(struct mock){0};assert(lefer_apply(g,0x1000,0,32,2,&e,&m,put,inv));assert(m.n==2&&m.values[0]==0x501);
 #define REJECT() do{m=(struct mock){0};memcpy(old,g,sizeof g);assert(!lefer_apply(g,0x1000,0,32,2,&e,&m,put,inv));assert(!m.n&&!memcmp(old,g,sizeof g));}while(0)
 g[0]=0x901;REJECT();g[0]=0x400;REJECT();g[0]=0x502;REJECT();g[0]=0xd01;e.ext_edx=0;REJECT();e.ext_edx=(1u<<11)|(1u<<20);
 e.cr0=0;REJECT();e.cr0=0x80000033;e.cr4=0;REJECT();e.cr4=0x20;e.entry=0;REJECT();e.entry=0x200;e.efer=0x100;REJECT();e.efer=0x500;
 m=(struct mock){0};assert(!lefer_apply(g,0x1000,3,32,2,&e,&m,put,inv));assert(!lefer_apply(g,0x1000,0,0x80000020,2,&e,&m,put,inv));assert(!lefer_apply(g,0x1000,0,32,1,&e,&m,put,inv));assert(!m.n);
 puts("linux_efer_commit: PASS");
}
