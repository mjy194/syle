#include "../firmware_port92.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned widths[]={0,1,2,3,4,8,16,UINT32_MAX};
 uint64_t high[]={0,UINT64_C(0x123456789abcde00),UINT64_C(0xffffffffffffff00)};
 /* All byte values and upper-bit patterns: reject reset bit0, disabled A20,
  * and every unimplemented device bit, without altering output storage. */
 for(unsigned h=0;h<sizeof high/sizeof*high;h++)for(unsigned byte=0;byte<256;byte++)for(unsigned input=0;input<2;input++){
  uint64_t v=high[h]|byte,old=v;bool expected=input||byte==2;
  assert(fp92_io(0x92,1,input,&v)==expected&&v==(input?(high[h]|2):old));
 }
 /* Exhaust every port with all supported and representative invalid widths. */
 for(unsigned port=0;port<=UINT16_MAX;port++)for(unsigned w=0;w<sizeof widths/sizeof*widths;w++)for(unsigned input=0;input<2;input++){
  uint64_t v=UINT64_C(0xaabbccddeeff0002);bool expected=port==0x92&&widths[w]==1;
  assert(fp92_io((uint16_t)port,widths[w],input,&v)==expected&&v==UINT64_C(0xaabbccddeeff0002));
 }
 /* Matching OVMF IoOr8 sequence, repeatability without hidden latch state. */
 uint64_t v=UINT64_C(0x123456789abcdeff);assert(fp92_io(0x92,1,true,&v));v|=2;assert(fp92_io(0x92,1,false,&v)&&v==UINT64_C(0x123456789abcde02));
 assert(fp92_io(0x92,1,false,&v)&&fp92_io(0x92,1,true,&v)&&v==UINT64_C(0x123456789abcde02));
 assert(!fp92_io(0x92,1,true,0)&&!fp92_io(0x92,1,false,0));
 puts("port92: fixed A20, exact idempotent OUT, exhaustive bytes/ports, no rejected mutation PASS");return 0;
}
