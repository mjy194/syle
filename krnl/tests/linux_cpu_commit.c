#include "../linux_cpu_commit.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct mock{unsigned n;bool fail;};
static bool put(void*p,uint32_t f,uint64_t v){struct mock*m=p;assert(f==0x681e&&v==0x1002);m->n++;return !m->fail;}
int main(void){
 struct lvp_cpu_features h={0xffffffff,0xffffffff,48};
 assert(lcpu_host_valid(1,0x756e6547,0x6c65746e,0x49656e69,0x80000008,&h));
 assert(!lcpu_host_valid(0,0x756e6547,0x6c65746e,0x49656e69,0x80000008,&h));
 assert(!lcpu_host_valid(1,0,0x6c65746e,0x49656e69,0x80000008,&h));
 assert(!lcpu_host_valid(1,0x756e6547,0x6c65746e,0x49656e69,0x80000007,&h));
 uint64_t g[15],old[15];memset(g,0xff,sizeof g);g[0]=0x100000001ULL;g[2]=0x100000000ULL;struct mock m={0};
 struct lvp_cpuid_result want;assert(lvp_cpuid(&h,1,0,&want));
 assert(lcpu_apply(g,0x1000,0,10,2,&h,&m,put));assert(m.n==1&&g[0]==want.a&&g[1]==want.b&&g[2]==want.c&&g[3]==want.d&&g[4]==UINT64_MAX);
 memcpy(old,g,sizeof g);m=(struct mock){.fail=true};assert(!lcpu_apply(g,0x1000,0,10,2,&h,&m,put));assert(!memcmp(old,g,sizeof g));
 #define BAD(cpl,reason,len) do{m=(struct mock){0};assert(!lcpu_apply(g,0x1000,cpl,reason,len,&h,&m,put));assert(!m.n&&!memcmp(old,g,sizeof g));}while(0)
 BAD(3,10,2);BAD(0,0x8000000a,2);BAD(0,10,1);BAD(0,32,2);
 h.basic_edx=0;BAD(0,10,2);h.basic_edx=0xffffffff;h.extended_edx=0;BAD(0,10,2);h.extended_edx=0xffffffff;h.physical_bits=31;BAD(0,10,2);
 h.physical_bits=48;m=(struct mock){0};assert(!lcpu_apply(g,0x7fffffffffffULL,0,10,2,&h,&m,put));assert(!m.n);
 puts("linux_cpu_commit: PASS");
}
