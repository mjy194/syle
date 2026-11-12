#include "../linux_high_pf_data.h"
#include <assert.h>
#include <stdio.h>
int main(void){const uint64_t nx=0x80000000000000e3ULL;
 assert(lhpd_metadata(0,0,nx,0xd01));assert(lhpd_metadata(62ULL<<32,0,nx,0xd01));
 assert(!lhpd_metadata(63ULL<<32,0,nx,0xd01));assert(!lhpd_metadata(UINT64_MAX,0,nx,0xd01));
 assert(!lhpd_metadata(1,0,nx,0xd01));assert(!lhpd_metadata(0,1,nx,0xd01));
 assert(!lhpd_metadata(0,0,0xe3,0xd01));assert(!lhpd_metadata(0,0,nx,0x501));
 for(unsigned i=0;i<64;i++)assert(!lhpd_metadata(0,0,nx^(1ULL<<i),0xd01));
 for(unsigned i=0;i<12;i++)assert(!lhpd_metadata(0,0,nx,0xd01^(1ULL<<i)));
 puts("linux_high_pf_data: PASS (host only)");}
