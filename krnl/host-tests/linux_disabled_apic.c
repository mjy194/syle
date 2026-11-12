#include "../linux_disabled_apic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned writes;static bool fail;
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;assert(f==0x681e&&v==0x1002);writes++;return !fail;}
int main(void){uint64_t g[15]={0},old[15];g[0]=g[3]=UINT64_MAX;g[2]=0xabcdef000000001bULL;
 assert(lda_read(LCP_BASELINE,0,1u<<5,g,0x1000,0,31,2,0,wr)&&g[0]==0xfee00100&&!g[3]&&writes==1);
 assert(!(g[0]&((1u<<10)|(1u<<11)))&&(g[0]&(1u<<8)));
 for(unsigned t=0;t<10;t++){memcpy(old,g,sizeof g);unsigned prior=writes;
 assert(!lda_read(t==0?LCP_EXPERIMENTAL_APIC:LCP_BASELINE,t==1?1u<<21:0,t==2?0:((1u<<5)|(t==3?1u<<9:0)),g,t==4?UINT64_MAX:t==5?0x7fffffffffffULL:0x1000,t==6?3:0,t==7?32:31,t==8?1:2,0,t==9?0:wr));
 assert(!memcmp(old,g,sizeof g)&&writes==prior);}
 g[2]=0x1c;assert(!lda_read(LCP_BASELINE,0,1u<<5,g,0x1000,0,31,2,0,wr));g[2]=0x1b;
 fail=true;memcpy(old,g,sizeof g);assert(!lda_read(LCP_BASELINE,0,1u<<5,g,0x1000,0,31,2,0,wr)&&!memcmp(old,g,sizeof g));
 puts("linux_disabled_apic: fixed disabled BSP MSR, capability/profile gate and write/failure atomicity PASS (host only)");return 0;}
