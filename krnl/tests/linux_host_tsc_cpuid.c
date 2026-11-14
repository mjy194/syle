#include "../linux_host_tsc_cpuid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool write(void*p,uint32_t f,uint64_t v){assert(f==0x681e&&v==0x1002);return p==0;}
int main(void){
 struct lvp_cpu_features h={UINT32_MAX,UINT32_MAX,32};struct lvp_cpuid_result r,b;
 assert(lht_query(&h,0,0,&r)&&r.a==0x15);assert(lvp_cpuid(&h,0,0,&b)&&b.a==1);
 assert(lht_query(&h,0x15,0,&r));assert((r.c/1000)*r.b/r.a==1193182);assert((uint64_t)r.c*r.b/r.a==1193182000);
 assert(lht_query(&h,0x80000007,0,&r)&&r.d==256);assert(lvp_cpuid(&h,0x80000007,0,&b)&&b.d==0);
 for(unsigned i=1;i<0x15;i++){assert(lht_query(&h,i,0,&r)&&lvp_cpuid(&h,i,0,&b)&&!memcmp(&r,&b,sizeof r));}
 uint64_t g[15]={0x15},old[15];memcpy(old,g,sizeof g);assert(!lht_apply(&h,g,0x1000,3,10,2,g,write)&&!memcmp(g,old,sizeof g));
 assert(lht_apply(&h,g,0x1000,3,10,2,0,write)&&g[0]==1000&&g[1]==1193182&&g[2]==1000000&&g[3]==0);
 assert(!lht_apply(&h,g,0x1000,1,10,2,0,write));assert(!lht_apply(&h,g,0x1000,0,10,3,0,write));
 puts("opt-in virtual TSC CPUID ratio/exact Linux kHz/baseline isolation/atomic write PASS");return 0;
}
