#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "linux_vcpu_policy.h"
int main(void){struct lvp_cpu_features h={UINT32_MAX,UINT32_MAX,48};struct lvp_cpuid_result r;
 assert(lvp_cpuid(&h,1,0,&r));uint32_t need=r.d;assert(r.c==0&&!(r.d&(1u<<9)));
 for(unsigned b=0;b<32;b++)if(need&(1u<<b)){h.basic_edx=UINT32_MAX^(1u<<b);assert(!lvp_cpu_supported(&h));}
 h.basic_edx=UINT32_MAX;
 assert(lvp_cpuid(&h,0x80000001,0,&r));need=r.d;for(unsigned b=0;b<32;b++)if(need&(1u<<b)){h.extended_edx=UINT32_MAX^(1u<<b);assert(!lvp_cpu_supported(&h));}
 h.extended_edx=UINT32_MAX;
 assert(lvp_cpuid(&h,7,0,&r));assert(!(r.a|r.b|r.c|r.d));assert(lvp_cpuid(&h,0x80000008,0,&r)&&r.a==(32|(48u<<8)));
 memset(&r,0x5a,sizeof r);struct lvp_cpuid_result old=r;h.physical_bits=31;assert(!lvp_cpuid(&h,1,0,&r)&&!memcmp(&old,&r,sizeof r));h.physical_bits=53;assert(!lvp_cpu_supported(&h));
 assert(lvp_guest_cr0(0x80010033)&&lvp_guest_cr4(0x6b0));assert(!lvp_guest_cr0(0x80010037)&&!lvp_guest_cr4(0x2020)&&!lvp_guest_cr4(0x40020));
 assert(!lvp_cpu_supported(NULL));puts("linux_vcpu_policy: PASS; minimal CPU profile only");}
