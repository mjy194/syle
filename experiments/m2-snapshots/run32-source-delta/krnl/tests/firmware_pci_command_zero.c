#include "firmware_pci_probe.h"
#include "firmware_piix_pm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct commit {unsigned calls;bool fail;};
static bool wr(void*p,uint32_t f,uint64_t v){struct commit*c=p;c->calls++;assert(f==0x681e&&(v==0x1001||v==0x1002));return !c->fail;}
int main(void){
 for(unsigned off=0;off<256;off++)for(unsigned width=1;width<=4;width*=2){
  if(off%width||width>256-off)continue;
  struct fpi_state a={0x80000000u|(off&~3u)},old=a;uint16_t port=0xcfc+(off&3);
  uint64_t mask=width==4?UINT64_C(0xffffffff):(UINT64_C(1)<<(width*8))-1;
  uint64_t value=UINT64_MAX&~mask,original=value;bool allowed=false;
#ifdef FPI_HOSTBRIDGE_COMMAND_ZERO
  allowed=off>=4&&off<6&&width<=6-off;
#endif
  assert(fpi_io(&a,port,width,false,&value)==allowed&&value==original&&!memcmp(&a,&old,sizeof a));
  for(unsigned bit=0;bit<width*8;bit++){value=UINT64_C(1)<<bit;assert(!fpi_io(&a,port,width,false,&value)&&value==(UINT64_C(1)<<bit));}
  if(off>=4&&off<=7){struct fpp_state s={0};assert(fpp_init(&s,7,100,1000));s.pci=a;
   uint64_t g[15]={original},saved[15];g[3]=port;memcpy(saved,g,sizeof g);
   uint64_t q=((uint64_t)port<<16)|(width==4?3:width==2?1:0);
   uint8_t ins[2]={width==2?0x66:width==1?0xee:0xef,0xef};unsigned len=width==2?2:1;struct commit c={0};
   assert(fpp_io(&s,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)==allowed&&c.calls==(allowed?1u:0u)&&!memcmp(g,saved,sizeof g)&&s.pci.address==a.address&&s.pm.last==101);
   if(allowed){s=(struct fpp_state){0};assert(fpp_init(&s,7,100,1000));s.pci=a;c=(struct commit){.fail=true};
    assert(!fpp_io(&s,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)&&s.failed&&c.calls==1&&!memcmp(g,saved,sizeof g)&&s.pci.address==a.address&&s.pm.last==101);
   }
  }
 }
 struct fpi_state s={0x80000004};uint64_t v=0;
 assert(!fpi_io(&s,0xcfd,2,false,&v)&&!fpi_io(&s,0xcfc,4,false,&v));
 v=UINT64_MAX;assert(fpi_io(&s,0xcfc,2,true,&v)&&(v&0xffff)==0); /* no command enable */
 puts("pci command zero: PASS zero-only command04/05, no status/BME grant");
}
