#include "firmware_pci_probe.h"
#include "firmware_piix_pm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct commit {unsigned calls;bool fail;};
static bool wr(void*p,uint32_t f,uint64_t v){struct commit*c=p;c->calls++;assert(f==0x681e&&(v==0x1001||v==0x1002));return !c->fail;}
int main(void){
 for(unsigned off=0x10;off<0x28;off++)for(unsigned w=1;w<=4;w*=2){
  if(off%w||w>0x28-off)continue;
  struct fpi_state s={0x80000000u|(off&~3u)};uint64_t r=UINT64_C(0x123456789abcdef0),old=r;
#ifdef FPI_HOSTBRIDGE_NO_BARS
  assert(fpi_io(&s,0xcfc+(off&3),w,true,&r));uint64_t mask=w==4?UINT64_C(0xffffffff):(UINT64_C(1)<<(w*8))-1;
  assert(r==(w==4?0:old&~mask));
#else
  assert(!fpi_io(&s,0xcfc+(off&3),w,true,&r)&&r==old);
#endif
  struct fpp_state p={0};assert(fpp_init(&p,7,100,1000));p.pci=s;
  uint64_t g[15]={old};g[3]=0xcfc+(off&3);uint8_t ins[2]={w==2?0x66:w==1?0xec:0xed,0xed};unsigned len=w==2?2:1;
  uint64_t q=(g[3]<<16)|8|(w==4?3:w==2?1:0);struct commit c={0};
#ifdef FPI_HOSTBRIDGE_NO_BARS
  assert(fpp_io(&p,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)&&c.calls==1&&g[0]==r);
  p=(struct fpp_state){0};assert(fpp_init(&p,7,100,1000));p.pci=s;g[0]=old;c=(struct commit){.fail=true};
  assert(!fpp_io(&p,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)&&p.failed&&g[0]==old&&p.pci.address==s.address&&c.calls==1&&p.pm.last==101);
#else
  assert(!fpp_io(&p,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)&&p.failed&&!c.calls&&g[0]==old);
#endif
  r=UINT64_MAX;assert(!fpi_io(&s,0xcfc+(off&3),w,false,&r)&&r==UINT64_MAX);
 }
 struct fpi_state s={0x80000024};uint64_t r=99;assert(!fpi_io(&s,0xcff,2,true,&r)&&r==99);assert(!fpi_io(&s,0xcfd,4,true,&r));
 for(unsigned off=0x28;off<256;off+=4){s.address=0x80000000|off;assert(!fpi_io(&s,0xcfc,4,true,&r));}
 s.address=0x80000810;r=0;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==UINT32_MAX); /* absent BDF unchanged */
 puts("pci no-bars: PASS read-only profile/default + fpp commit failures");
}
