#include "../linux_pf_diagnostic.h"
#include "../linux_guest_session.h"
#include <assert.h>
#include <stdio.h>
static struct lgs_session s;static unsigned char mem[6][4096];static unsigned maps;static bool fields=true;
static void*map(void*c,uint64_t pa){(void)c;maps++;return pa>=0x1000&&pa<=0x6000?mem[pa/4096-1]:0;}
static bool readfield(void*c,uint32_t f,uint64_t*v){(void)c;if(!fields)return false;*v=0;switch(f){case 0x4404:*v=0x80000b0e;break;case 0x4016:*v=0xb0e;break;case 0x4018:*v=6;break;case 0x401a:*v=7;break;case 0x4408:*v=0x123;break;case 0x4824:*v=2;break;case 0x4826:*v=1;break;case 0x6802:*v=0x1000;break;case 0x6400:case 0x681e:*v=0x5000;break;}return true;}
int main(void){s.memory.prepared=s.host.active=s.host.ready=true;s.memory.owner.map=map;s.platform.read_field=readfield;s.native.gpr[11]=123;
 for(unsigned i=1;i<=6;i++)s.memory.pages[i]=i*4096;
 ((uint64_t*)mem[0])[0]=0x2003;((uint64_t*)mem[1])[0]=0x3003;((uint64_t*)mem[2])[0]=0x4003;((uint64_t*)mem[3])[5]=0x5003;
 struct lgpd_snapshot d;assert(lgpd_collect(&s,&d));assert(d.valid==((UINT64_C(1)<<LGPD_FIELDS)-1)&&d.r12==123&&d.trace[0].status==1&&d.trace[0].gpa==0x5000);assert(d.data_valid&4);assert(d.value[28]==0xb0e&&d.value[29]==6&&d.value[30]==7&&d.value[31]==0x123&&d.value[32]==2&&d.value[33]==1);assert(maps<1000);
 fields=false;maps=0;assert(lgpd_collect(&s,&d));assert(!d.valid&&!maps);fields=true;
 s.memory.pages[1]=0;assert(lgpd_collect(&s,&d));assert(d.trace[0].status!=1);
 s.state.host_dirty=true;assert(!lgpd_collect(&s,&d));puts("PF diagnostic: bounded owned translation, invalid fields/tokens, dirty host rejection PASS");}
