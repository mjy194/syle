#include "firmware_pci_probe.h"
#include "firmware_piix_pm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct commit {unsigned calls;bool fail;};
static bool wr(void*p,uint32_t f,uint64_t v){struct commit*c=p;c->calls++;assert(f==0x681e&&(v==0x1001||v==0x1002));return !c->fail;}
int main(void){
 const uint8_t hdr[16]={0x86,0x80,0x37,0x12,0,0,0,0,2,0,0,6,0,0,0,0};
 for(unsigned off=0;off<256;off++)for(unsigned width=1;width<=4;width*=2){
  if(off%width||width>256-off)continue;
  struct fpi_state a={0x80000000u|(off&~3u)};uint16_t port=0xcfc+(off&3);
  uint64_t old=UINT64_C(0x123456789abcdef0),value=old;bool admitted=off<16;
#ifdef FPI_HOSTBRIDGE_READONLY_CONFIG
  admitted=true;
#endif
  uint32_t bytes=0;for(unsigned i=0;i<width;i++)if(off+i<16)bytes|=(uint32_t)hdr[off+i]<<(8*i);
  uint64_t mask=width==4?UINT64_C(0xffffffff):(UINT64_C(1)<<(width*8))-1;
  uint64_t expected=width==4?bytes:(old&~mask)|bytes;
  assert(fpi_io(&a,port,width,true,&value)==admitted&&value==(admitted?expected:old));
  value=UINT64_MAX;uint32_t address=a.address;assert(!fpi_io(&a,port,width,false,&value)&&value==UINT64_MAX&&a.address==address);
  struct fpp_state s={0};assert(fpp_init(&s,7,100,1000));s.pci=a;
  uint64_t g[15]={old};g[3]=port;uint64_t q=((uint64_t)port<<16)|8|(width==4?3:width==2?1:0);
  uint8_t ins[2]={width==2?0x66:width==1?0xec:0xed,0xed};unsigned len=width==2?2:1;struct commit c={0};
  assert(fpp_io(&s,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)==admitted);
  assert(g[0]==(admitted?expected:old)&&c.calls==(admitted?1u:0u));
  if(admitted){s=(struct fpp_state){0};assert(fpp_init(&s,7,100,1000));s.pci=a;g[0]=old;c=(struct commit){.fail=true};
   assert(!fpp_io(&s,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)&&s.failed&&g[0]==old&&s.pci.address==address&&s.pm.last==101&&c.calls==1);
  }
 }
 struct fpi_state s={0x800000fc};uint64_t r=99;assert(!fpi_io(&s,0xcff,2,true,&r)&&r==99);assert(!fpi_io(&s,0xcfd,4,true,&r));
 assert(!fpi_io(&s,0xd00,1,true,&r)&&!fpi_io(&s,0xcfc,8,true,&r));
 for(unsigned off=0;off<256;off+=4){s.address=0x80000800|off;r=0;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==UINT32_MAX);}
 s.address=0x10;r=0;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==UINT32_MAX);
 puts("pci synthetic type0 256: PASS all offsets/widths + fpp failures");
}
