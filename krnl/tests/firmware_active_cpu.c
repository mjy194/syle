#include "../firmware_active_cpu.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){struct fcp_host h={~0u,~0u,1,0x80000008u,48,48};struct fcp_result r,b;
 assert(fca_supported(&h));assert(fca_query(&h,1,0,&r)&&fcp_query(&h,1,0,&b));
 assert((r.d^(1u<<9))==b.d&&r.c==b.c&&!(r.c&(1u<<21)));
 assert(fca_query(&h,7,0,&r)&&(r.d&(1u<<9)));
 assert(fca_query(&h,0x80000001,0,&r)&&r.d==(1u<<29));
 assert(fca_query(&h,0,0,&r)&&r.a==1);
 h.basic_edx&=~(1u<<9);assert(!fca_supported(&h));memset(&r,0x55,sizeof r);b=r;
 assert(!fca_query(&h,1,0,&r)&&!memcmp(&r,&b,sizeof r));
 puts("firmware active CPU: APIC exposure/host gate/x2 hidden/fallback PASS");}
