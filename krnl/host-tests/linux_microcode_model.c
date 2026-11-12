#include "../linux_microcode_model.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct mock {unsigned writes;bool fail;uint64_t rip;};
static bool wr(void*p,uint32_t field,uint64_t value){struct mock*m=p;assert(field==0x681e);m->writes++;m->rip=value;return !m->fail;}
int main(void){
 struct lmc_state s={0};uint64_t g[15]={0};struct mock m={0};
 struct lvp_cpu_features h={.basic_edx=UINT32_MAX,.extended_edx=UINT32_MAX,.physical_bits=32};
 /* Index and write payload use architectural low 32 bits. */
 g[2]=0xfeed0000008bull;g[0]=g[3]=0xbeef00000000ull;
 assert(lmc_msr(&s,g,0x1000,0,32,2,&m,wr)&&!s.loaded&&m.rip==0x1002);
 for(unsigned n=0;n<3;n++){
  g[0]=0xaaaa00000001ull;assert(lmc_cpuid(&s,g,0x1000,0,10,2,&h,&m,wr)&&s.loaded);
  g[2]=0x8b;g[0]=g[3]=UINT64_MAX;assert(lmc_msr(&s,g,0x1000,0,31,2,&m,wr)&&g[0]==0&&g[3]==0&&s.loaded);
  assert(lmc_msr(&s,g,0x1000,0,32,2,&m,wr)&&!s.loaded);
 }
 s.loaded=true;g[2]=0x8b;g[0]=g[3]=0;m.fail=true;uint64_t before[15];memcpy(before,g,sizeof g);
 assert(!lmc_msr(&s,g,0x1000,0,32,2,&m,wr)&&s.loaded&&!memcmp(g,before,sizeof g));
 g[0]=1;s.loaded=false;memcpy(before,g,sizeof g);
 assert(!lmc_cpuid(&s,g,0x1000,0,10,2,&h,&m,wr)&&!s.loaded&&!memcmp(g,before,sizeof g));
 m.fail=false;g[2]=0x8b;g[0]=g[3]=0;
 unsigned writes=m.writes;
 for(unsigned c=1;c<=3;c++)assert(!lmc_msr(&s,g,0x1000,c,31,2,&m,wr));
 for(unsigned len=0;len<=16;len++)if(len!=2)assert(!lmc_msr(&s,g,0x1000,0,31,len,&m,wr));
 g[2]=0x79;assert(!lmc_msr(&s,g,0x1000,0,32,2,&m,wr));g[2]=0x8c;assert(!lmc_msr(&s,g,0x1000,0,31,2,&m,wr));g[2]=0x8b;
 g[0]=1;assert(!lmc_msr(&s,g,0x1000,0,32,2,&m,wr));g[0]=0;g[3]=1;assert(!lmc_msr(&s,g,0x1000,0,32,2,&m,wr));g[3]=0;
 assert(!lmc_msr(&s,g,0x800000000000ull,0,31,2,&m,wr));
 assert(!lmc_msr(&s,g,0x7fffffffffffull,0,31,2,&m,wr));
 assert(!lmc_msr(&s,g,UINT64_MAX,0,31,2,&m,wr));
 assert(!lmc_msr(&s,g,0x1000,0,10,2,&m,wr));assert(m.writes==writes);
 assert(lmc_msr(&s,g,0xffff800000000000ull,0,31,2,&m,wr));
 g[0]=0;s.loaded=false;assert(lmc_cpuid(&s,g,0x1000,0,10,2,&h,&m,wr)&&!s.loaded);
 puts("linux_microcode_model: virtual revision zero, transactions and validation PASS (host only)");
}
