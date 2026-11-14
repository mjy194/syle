"""Extract actual transformed init/dispatch statements for host seam test.
Not an execution of VM-exit, clock_sync, owned fetch or hardware VMCS writes.
"""
import pathlib,re,sys
s=pathlib.Path(sys.argv[1]).read_text();out=pathlib.Path(sys.argv[2]);assert not out.exists()
init=re.findall(r'if\(!vfb_init_nodma_probe[^\n]+',s);assert len(init)==1
io=re.findall(r'if\(enumeration_cycles\+\+>=4096\)[^\n]+\n  uint64_t before=native.gpr\[0\];bool accepted=vfb_io\([^\n]+',s);assert len(io)==1
out.write_text('''#include "pci_probe_nodma.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct vfb enumeration;static unsigned enumeration_cycles;
static struct {uint64_t gpr[15];}native;
static struct {uint64_t epoch,reference;}active_apic={1,0};
static struct {uint64_t max_gap;}clock_source={100};
static bool pic_write(void*x,uint32_t f,uint64_t v){(void)x;assert(f==0x681e&&(v==0x1001||v==0x1002));return true;}
static bool begin(void){'''+init[0]+'''return true;}
static bool dispatch(unsigned port,unsigned width,bool input,uint64_t value){
 uint64_t rip=0x1000,cs=0,q=((uint64_t)port<<16)|(width==4?3:width==2?1:0)|(input?8:0);unsigned len=0;uint8_t io_bytes[2];
 if(width==2)io_bytes[len++]=0x66;io_bytes[len++]=(uint8_t)(0xec+(input?0:2)+(width==1?0:1));unsigned io_available=len;native.gpr[0]=value;native.gpr[3]=port;
 '''+io[0]+'''
 (void)before;return accepted;
}
int main(void){assert(begin());assert(dispatch(0xcf8,4,false,0x80000000));assert(dispatch(0xcfe,2,true,0x12340000)&&native.gpr[0]==0x12341237);
 assert(dispatch(0xcf8,4,false,0x80001010)&&dispatch(0xcfc,4,false,0xd001));assert(dispatch(0xcf8,4,false,0x80001004)&&dispatch(0xcfc,2,false,5));assert(dispatch(0xd012,1,false,3));assert(!dispatch(0xd008,4,false,1)&&enumeration.failed&&!enumeration.disk.pfn);
 memset(&enumeration,0,sizeof enumeration);assert(begin());enumeration_cycles=4096;assert(!dispatch(0xcf8,4,false,0)&&enumeration.failed);
 puts("actual transformed observer init/dispatch statements: host shadow PASS");}
''')
