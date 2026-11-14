#include "firmware_cpu_profile.h"
#define B(n) (UINT32_C(1)<<(n))
/* No MCA banks, APIC, TSC, MTRR, PAT, PGE, XSAVE or accelerated extensions. */
#define BASIC (B(0)|B(5)|B(6)|B(7)|B(8)|B(15)|B(23)|B(24)|B(25)|B(26))
#define EXT B(29)
bool fcp_efer_allowed(uint64_t v){return !(v&~UINT64_C(0x500));}
bool fcp_supported(const struct fcp_host*h){return h&&h->max_basic>=1&&h->max_extended>=0x80000008u&&h->physical_bits>=32&&h->physical_bits<=52&&h->virtual_bits>=48&&h->virtual_bits<=57&&(h->basic_edx&BASIC)==BASIC&&(h->extended_edx&EXT)==EXT;}
bool fcp_query(const struct fcp_host*h,uint32_t leaf,uint32_t sub,struct fcp_result*out){
 if(!out||!fcp_supported(h))return false;
 struct fcp_result r={0};(void)sub; /* All advertised leaves are non-indexed. */
 /* Intel out-of-range inputs return the highest basic leaf. */
 if((leaf>1&&leaf<0x80000000u)||leaf>0x80000008u)leaf=1;
 switch(leaf){
 case 0:r=(struct fcp_result){1,0x756e6547,0x6c65746e,0x49656e69};break;
 case 1:r.a=0x600;r.b=1u<<16;r.d=BASIC;break;
 case 0x80000000:r.a=0x80000008;break;
 case 0x80000001:r.d=EXT;break;
 case 0x80000008:r.a=32|(48u<<8);break;
 default:break; /* Reserved extended leaves 2..7. */
 }
 *out=r;return true;
}
