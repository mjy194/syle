#include "../firmware_fwcfg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){struct ffc_state s={0};assert(ffc_init(&s,0x8000000,1));unsigned keys[]={0x17,8,0xb,0x14};
 for(unsigned k=0;k<4;k++){uint64_t r=UINT64_C(0x123456789abc0000)|keys[k],old=r;struct ffc_state before=s;
#ifdef FFC_NO_KERNEL_BLOBS
  (void)before;assert(ffc_exit(&s,0x5100001,&r)&&r==old&&s.selector==keys[k]&&!s.cursor);
  for(unsigned i=0;i<9;i++){r=UINT64_C(0xabcdef12345678aa);assert(ffc_io(&s,0x511,1,true,&r)&&r==UINT64_C(0xabcdef1234567800)&&s.cursor==i+1);}
  r=keys[k];assert(ffc_io(&s,0x510,2,false,&r)&&!s.cursor);
  s.cursor=UINT64_MAX;before=s;assert(!ffc_io(&s,0x511,1,true,&r)&&!memcmp(&s,&before,sizeof s));s.cursor=0;
#else
  assert(!ffc_exit(&s,0x5100001,&r)&&r==old&&!memcmp(&s,&before,sizeof s));
#endif
 }
 unsigned deny[]={7,9,0xa,0xc,0x10,0x11,0x12,0x13,0x15,0x16,0x18,0x4017,0x8017,0xffff};
 for(unsigned i=0;i<sizeof deny/sizeof *deny;i++){uint64_t r=deny[i];struct ffc_state old=s;assert(!ffc_io(&s,0x510,2,false,&r)&&r==deny[i]&&!memcmp(&s,&old,sizeof s));}
 uint64_t r=0;assert(!ffc_io(&s,0x514,8,false,&r));assert(!ffc_io(&s,0x511,1,false,&r));assert(!ffc_io(&s,0x510,1,false,&r));
 puts("firmware_fwcfg_no_blobs: PASS");}
