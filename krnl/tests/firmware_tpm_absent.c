#include "../firmware_tpm_absent.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned widths[]={0,1,2,3,4,8,16,UINT32_MAX};
 uint64_t initial[]={0,1,UINT64_MAX,UINT64_C(0x123456789abcdef0)};
 for(unsigned offset=0;offset<0x10000;offset++)for(unsigned w=0;w<sizeof widths/sizeof*widths;w++)for(unsigned write=0;write<2;write++)for(unsigned i=0;i<sizeof initial/sizeof*initial;i++){
  uint64_t v=initial[i];bool expected=offset==0&&widths[w]==1&&!write;
  assert(fta_mmio(UINT64_C(0xfed40000)+offset,widths[w],write,&v)==expected&&v==(expected?0xff:initial[i]));
 }
 uint64_t bad[]={0,UINT64_MAX,UINT64_C(0xfed3ffff),UINT64_C(0xfed50000),UINT64_C(0x1fed40000),UINT64_C(0xfffffffffed40000)};
 for(unsigned i=0;i<sizeof bad/sizeof*bad;i++){uint64_t v=0x1234;assert(!fta_mmio(bad[i],1,false,&v)&&v==0x1234);}
 for(unsigned bit=0;bit<64;bit++){uint64_t v=0x1234;assert(!fta_mmio(UINT64_C(0xfed40000)^(UINT64_C(1)<<bit),1,false,&v)&&v==0x1234);}
 assert(!fta_mmio(0xfed40000,1,false,0));
 puts("TPM absent: exact base byte FF, all writes/other widths/addresses reject unchanged PASS");return 0;
}
