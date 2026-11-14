#include "pci_probe_nodma.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static bool commit(void*x,uint32_t f,uint64_t v){(void)x;assert(f==0x681e&&(v==0x1001||v==0x1002));return true;}
static bool out(struct vfb*s,unsigned p,unsigned n,uint32_t v){unsigned z=n==4?3:n==2?1:0,len=0;uint8_t b[2];if(n==2)b[len++]=0x66;b[len++]=(uint8_t)(0xee +(n==1?0:1));uint64_t g[15]={0};g[0]=v;g[3]=p;return vfb_io(s,g,0x1000,0,((uint64_t)p<<16)|z,len,1,0,b,len,true,0,commit);}
int main(void){struct vfb s={0};assert(vfb_init_nodma_probe(&s,1,0,100));
 uint8_t sentinel[8];memset(sentinel,0x7c,sizeof sentinel);
 assert(!s.disk.io.admit(0,0,4096,true));assert(!s.disk.io.read(0,0,sentinel,sizeof sentinel));for(unsigned i=0;i<8;i++)assert(sentinel[i]==0x7c);
 assert(!s.disk.io.write(0,0,(const void*)(uintptr_t)1,SIZE_MAX)); /* pointer never dereferenced */
 assert(out(&s,0xcf8,4,0x80001010)&&out(&s,0xcfc,4,0xd001));assert(out(&s,0xcf8,4,0x80001004)&&out(&s,0xcfc,2,5));assert(out(&s,0xd012,1,3));
 assert(!out(&s,0xd008,4,1)&&s.failed&&!s.disk.pfn);assert(!out(&s,0xd010,2,0));
 puts("enumeration-only no-DMA callback refusal and PFN terminal: host PASS");
}
