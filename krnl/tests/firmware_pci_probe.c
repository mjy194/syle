#include "../firmware_pci_probe.h"
#include <assert.h>
#include <stdio.h>
static void no(struct fpi_state*s,unsigned p,unsigned w,bool in){struct fpi_state old=*s;uint64_t r=0x123456789abcdef0,save=r;assert(!fpi_io(s,p,w,in,&r));assert(s->address==old.address&&r==save);}
int main(void){struct fpi_state s={0};uint64_t r=~0ull;
 assert(fpi_io(&s,0xcf8,4,true,&r)&&r==0);
 r=~0ull;assert(fpi_io(&s,0xcf8,4,false,&r)&&s.address==0x80fffffc&&r==~0ull);
 r=0x80000000;assert(fpi_io(&s,0xcf8,4,false,&r));r=~0ull;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==0x12378086);
 r=0xaabbccddeeff0000;assert(fpi_io(&s,0xcfc,2,true,&r)&&r==0xaabbccddeeff8086);
 assert(fpi_io(&s,0xcff,1,true,&r)&&r==0xaabbccddeeff8012);
 s.address=0x80000008;r=0;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==0x06000002);
 s.address=0x80000004;r=~0ull;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==0);
 no(&s,0xcfc,4,false);s.address=0x80000010;no(&s,0xcfc,4,true);
 const uint32_t absent[]={0,0x80000100,0x80000800,0x80010000,0x80ffff00};
 for(unsigned i=0;i<sizeof absent/sizeof absent[0];i++){s.address=absent[i];r=0;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==0xffffffff);assert(fpi_io(&s,0xcfc,4,false,&r)&&r==0xffffffff&&s.address==absent[i]);}
 no(&s,0xcf8,1,true);no(&s,0xcf8,2,false);no(&s,0xcfd,2,true);no(&s,0xcff,4,true);no(&s,0xcf9,1,true);no(&s,0xcfc,3,true);no(&s,0x80,1,false);
 assert(!fpi_io(0,0xcf8,4,true,&r)&&!fpi_io(&s,0xcf8,4,true,0));
 s.address=0x80000000;
 for(unsigned lane=0;lane<4;lane++){r=~0ull;assert(fpi_io(&s,0xcfc+lane,1,true,&r));assert((r&~255ull)==(~0ull&~255ull));}
 r=0;assert(fpi_io(&s,0xcfe,2,true,&r)&&r==0x1237);
 s.address=0x8000000c;r=1;assert(fpi_io(&s,0xcfc,4,true,&r)&&r==0);
 r=~0ull;assert(fpi_exit(&s,0x0cf8000b,&r)&&r==s.address);
 r=~0ull;assert(fpi_exit(&s,0x0cf8004b,&r)&&r==s.address);
 const uint64_t invalid[]={0x0cf8000a,0x0cf8001b,0x0cf8002b,0x0cf8008b,0x10cf8000bull};
 for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){uint32_t addr=s.address;r=0xfeed123456789abc;assert(!fpi_exit(&s,invalid[i],&r)&&r==0xfeed123456789abc&&s.address==addr);}
 puts("firmware PCI probe: latch/identity/absent/width/register preservation/reject PASS");}
