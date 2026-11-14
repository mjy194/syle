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
  uint16_t port=0xcfc+(off&3);struct fpi_state a={0x80000000u|(off&~3u)},before=a;
  uint64_t mask=width==4?UINT64_C(0xffffffff):(UINT64_C(1)<<(8*width))-1;
  for(unsigned pattern=0;pattern<=width*8+1;pattern++){
   uint64_t value=pattern==0?0:pattern==width*8+1?mask:UINT64_C(1)<<(pattern-1);
   value|=UINT64_C(0xabcdef0000000000);uint64_t old=value;bool allowed=false;
#ifdef FPI_HOSTBRIDGE_ENUM_NO_RESOURCES
   allowed=off<64;
#elif defined(FPI_HOSTBRIDGE_COMMAND_ZERO)
   allowed=off>=4&&off<6&&width<=6-off&&(value&mask)==0;
#endif
   uint64_t read_before=0;bool readable=fpi_io(&a,port,width,true,&read_before);
   assert(fpi_io(&a,port,width,false,&value)==allowed&&value==old&&!memcmp(&a,&before,sizeof a));
   uint64_t read_after=0;assert(fpi_io(&a,port,width,true,&read_after)==readable&&read_before==read_after);
   struct fpp_state s={0};assert(fpp_init(&s,7,100,1000));s.pci=a;
   uint64_t g[15]={old};g[3]=port;uint64_t q=((uint64_t)port<<16)|(width==4?3:width==2?1:0);
   uint8_t ins[2]={width==2?0x66:width==1?0xee:0xef,0xef};unsigned len=width==2?2:1;struct commit c={0};
   assert(fpp_io(&s,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)==allowed&&c.calls==(allowed?1u:0u)&&g[0]==old&&s.pci.address==a.address&&s.pm.last==101);
   if(allowed){s=(struct fpp_state){0};assert(fpp_init(&s,7,100,1000));s.pci=a;c=(struct commit){.fail=true};
    assert(!fpp_io(&s,g,0x1000,0,q,len,7,101,ins,len,true,&c,wr)&&s.failed&&c.calls==1&&g[0]==old&&s.pci.address==a.address&&s.pm.last==101);}
  }
 }
#ifdef FPI_HOSTBRIDGE_ENUM_NO_RESOURCES
 /* Enumerating all-ones BAR probe/restore and command27 never enables decode. */
 for(unsigned off=0x10;off<=0x30;off+=4){if(off==0x28||off==0x2c)continue;struct fpi_state a={0x80000000|off};uint64_t v=UINT32_MAX;assert(fpi_io(&a,0xcfc,4,false,&v));v=99;assert(fpi_io(&a,0xcfc,4,true,&v)&&!v);assert(fpi_io(&a,0xcfc,4,false,&v));}
 struct fpi_state a={0x80000004};uint64_t v=0x27;assert(fpi_io(&a,0xcfc,2,false,&v));assert(fpi_io(&a,0xcfc,2,true,&v)&&!v);assert(fpi_io(&a,0xcfc,2,false,&v));
#endif
 struct fpi_state a2={0x8000003c};uint64_t v2=0;assert(!fpi_io(&a2,0xcff,2,false,&v2));a2.address=0x80000810;assert(fpi_io(&a2,0xcfc,4,false,&v2));assert(fpi_io(&a2,0xcfc,4,true,&v2)&&v2==UINT32_MAX);
 puts("pci enum no-resources: PASS full header bit patterns/fpp fault transactions");
}
