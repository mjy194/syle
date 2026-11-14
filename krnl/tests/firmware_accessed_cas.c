#include "../firmware_accessed_cas.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 for(unsigned d=0;d<2;d++){
  uint64_t old=UINT64_C(0xfee00083)|(d?0x40:0),x=old;
  assert(faa_atomic_cas(&x,old,old|0x20)==FWD_CAS_OK&&x==(old|0x20));
  assert((x&0x40)==(old&0x40));
  assert(faa_atomic_cas(&x,x,x)==FWD_CAS_CONFLICT);
  for(unsigned bit=0;bit<64;bit++){
   x=old^(UINT64_C(1)<<bit);uint64_t before=x;
   assert(faa_atomic_cas(&x,old,old|0x20)==FWD_CAS_CONFLICT&&x==before);
  }
  x=old;assert(faa_atomic_cas(&x,old,(old|0x20)^0x40)==FWD_CAS_CONFLICT&&x==old);
 }
 assert(faa_atomic_cas(0,0,0x20)==FWD_CAS_CONFLICT);
 _Alignas(8) unsigned char b[24];assert(faa_atomic_cas((uint64_t*)(void*)(b+1),0,0x20)==FWD_CAS_CONFLICT);
 puts("accessed CAS: exact A-only, D unchanged, aligned seq_cst, no retry PASS");
}
