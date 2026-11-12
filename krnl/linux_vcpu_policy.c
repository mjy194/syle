#include "linux_vcpu_policy.h"
#define BASIC ((1u<<0)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6)|(1u<<8)|(1u<<13)|(1u<<15)|(1u<<24)|(1u<<25)|(1u<<26))
#define EXT ((1u<<11)|(1u<<20)|(1u<<29))
bool lvp_cpu_supported(const struct lvp_cpu_features *h){return h&&h->physical_bits>=32&&h->physical_bits<=52&&(h->basic_edx&BASIC)==BASIC&&(h->extended_edx&EXT)==EXT;}
bool lvp_cpuid(const struct lvp_cpu_features *h,uint32_t leaf,uint32_t sub,struct lvp_cpuid_result *out){
 if(!out||!lvp_cpu_supported(h))return false;
 struct lvp_cpuid_result r={0};(void)sub;
 switch(leaf){case 0:r.a=1;r.b=0x756e6547;r.d=0x49656e69;r.c=0x6c65746e;break;
 case 1:r.a=0x00000600;r.b=1u<<16;r.d=BASIC;break;
 case 0x80000000:r.a=0x80000008;break;case 0x80000001:r.d=EXT;break;
 case 0x80000008:r.a=32|(48u<<8);break;default:break;}
 *out=r;return true;
}
bool lvp_guest_cr0(uint64_t v){const uint64_t required=0x80000031u,allowed=0x8005003bu;return (v&required)==required&&!(v&~allowed);}
bool lvp_guest_cr4(uint64_t v){return (v&0x20)&&!(v&~UINT64_C(0x6b0));}
