#include "../firmware_dirty_cas.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint64_t x=0xffc000a3;assert(fwd_atomic_cas(&x,x,x|0x40)==FWD_CAS_OK&&x==0xffc000e3);
 assert(fwd_atomic_cas(&x,0xffc000a3,0xffc000e3)==FWD_CAS_CONFLICT&&x==0xffc000e3);
 for(unsigned b=0;b<64;b++){uint64_t old=0xffc000a3;x=old^(UINT64_C(1)<<b);uint64_t before=x;
  assert(fwd_atomic_cas(&x,old,old|0x40)==FWD_CAS_CONFLICT&&x==before);
 }
 x=0xffc000a3;assert(fwd_atomic_cas(&x,x,x|0x42)==FWD_CAS_OK); /* RW already1: exact desired still onlyD. */
 x=0xffc000a3;assert(fwd_atomic_cas(&x,x,x^1)==FWD_CAS_CONFLICT&&x==0xffc000a3);
 assert(fwd_atomic_cas(0,0,0x40)==FWD_CAS_CONFLICT);
 _Alignas(8) unsigned char b[24];assert(fwd_atomic_cas((uint64_t*)(void*)(b+1),0,0x40)==FWD_CAS_CONFLICT);
 puts("dirty CAS: aligned seq_cst compare-exchange, exact D-only change, no retry PASS");
}
