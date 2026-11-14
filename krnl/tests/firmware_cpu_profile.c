#include "../firmware_cpu_profile.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){struct fcp_host h={~0u,~0u,1,0x80000008u,48,48};struct fcp_result r,old;
 assert(fcp_query(&h,0,99,&r)&&r.a==1&&r.b==0x756e6547&&r.c==0x6c65746e&&r.d==0x49656e69);
 assert(fcp_query(&h,1,0,&r)&&r.a==0x600&&r.b==1u<<16&&!r.c);uint32_t basic=r.d;
 assert((basic&(1u<<7))&&!(basic&((1u<<9)|(1u<<14)|(1u<<4)|(1u<<12)|(1u<<16))));
 for(unsigned i=0;i<32;i++)if(basic&(1u<<i)){struct fcp_host n=h;n.basic_edx&=~(1u<<i);memset(&r,0x55,sizeof r);old=r;assert(!fcp_query(&n,0,0,&r));assert(!memcmp(&r,&old,sizeof r));}
 assert(fcp_query(&h,0x80000000,0,&r)&&r.a==0x80000008&&!r.b&&!r.c&&!r.d);
 assert(fcp_query(&h,0x80000001,0,&r)&&r.d==1u<<29&&!r.a&&!r.b&&!r.c);
 for(uint32_t l=0x80000002;l<0x80000008;l++){assert(fcp_query(&h,l,0,&r));assert(!(r.a|r.b|r.c|r.d));}
 assert(fcp_query(&h,0x80000008,0,&r)&&r.a==(32|(48u<<8))&&!r.b&&!r.c&&!r.d);
 const uint32_t outside[]={2,7,0xd,0x21,0x40000000,0x8000001f,0xffffffff};
 for(unsigned i=0;i<sizeof outside/sizeof outside[0];i++)assert(fcp_query(&h,outside[i],1,&r)&&r.a==0x600&&r.d==basic&&!r.c);
 h.max_basic=0;memset(&r,0x55,sizeof r);old=r;assert(!fcp_query(&h,0,0,&r)&&!memcmp(&r,&old,sizeof r));h.max_basic=1;
 h.max_extended=0x80000007;assert(!fcp_query(&h,0,0,&r)&&!memcmp(&r,&old,sizeof r));h.max_extended=0x80000008;
 h.extended_edx=0;assert(!fcp_supported(&h));h.extended_edx=~0u;
 h.physical_bits=31;assert(!fcp_supported(&h));h.physical_bits=53;assert(!fcp_supported(&h));h.physical_bits=32;
 h.virtual_bits=47;assert(!fcp_supported(&h));h.virtual_bits=58;assert(!fcp_supported(&h));h.virtual_bits=48;assert(fcp_supported(&h));
 assert(!fcp_query(0,0,0,&r)&&!fcp_query(&h,0,0,0));
 assert(fcp_efer_allowed(0)&&fcp_efer_allowed(0x100)&&fcp_efer_allowed(0x500));
 assert(!fcp_efer_allowed(1)&&!fcp_efer_allowed(0x800)&&!fcp_efer_allowed(1ull<<63));
 puts("firmware CPU profile: maxima/features/host-gates/reserved/fallback/EFER PASS");}
