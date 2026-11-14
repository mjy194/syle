#include "../firmware_cpuhp_absent.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
 uint64_t samples[]={0,1,0xff,0x100,0x10000,0x1000000,UINT64_C(1)<<31,UINT64_C(1)<<32,UINT64_C(0xffffffff00000000),UINT64_MAX};
 unsigned widths[]={0,1,2,3,4,5,8,16,UINT32_MAX};
 for(unsigned port=0;port<=UINT16_MAX;port++)for(unsigned w=0;w<sizeof widths/sizeof*widths;w++)for(unsigned in=0;in<2;in++)for(unsigned j=0;j<sizeof samples/sizeof*samples;j++){
  uint64_t v=samples[j];bool expected=(port==0xaf00&&widths[w]==4&&(in||(uint32_t)v==0))||(port==0xaf05&&widths[w]==1&&!in&&(uint8_t)v==0);
  assert(fcha_io((uint16_t)port,widths[w],in,&v)==expected);
  assert(v==((expected&&in)?UINT64_C(0xffffffff):samples[j]));
 }
 /* Exact firmware sequence: repeated zero writes do not create state. */
 uint64_t v=0;assert(fcha_io(0xaf00,4,false,&v)&&v==0);assert(fcha_io(0xaf00,4,false,&v)&&v==0);assert(fcha_io(0xaf05,1,false,&v)&&v==0);
 v=UINT64_MAX;assert(fcha_io(0xaf00,4,true,&v)&&v==UINT64_C(0xffffffff));
 for(unsigned b=0;b<32;b++){v=UINT64_C(1)<<b;assert(!fcha_io(0xaf00,4,false,&v)&&v==(UINT64_C(1)<<b));}
 for(unsigned b=0;b<8;b++){v=UINT64_C(1)<<b;assert(!fcha_io(0xaf05,1,false,&v)&&v==(UINT64_C(1)<<b));}
 assert(!fcha_io(0xaf00,4,true,0)&&!fcha_io(0xaf00,4,false,0)&&!fcha_io(0xaf05,1,false,0));
 puts("CPU hotplug absent probe: exact forms, exhaustive ports/widths/directions, no rejected mutation PASS");return 0;
}
