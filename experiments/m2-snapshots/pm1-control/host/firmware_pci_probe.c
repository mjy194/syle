#include "firmware_pci_probe.h"
#if defined(FPI_HOSTBRIDGE_ENUM_NO_RESOURCES) && !defined(FPI_HOSTBRIDGE_READONLY_CONFIG)
#error FPI_HOSTBRIDGE_ENUM_NO_RESOURCES_requires_READONLY_CONFIG
#endif
#if defined(FPI_HOSTBRIDGE_COMMAND_ZERO) && !defined(FPI_HOSTBRIDGE_READONLY_CONFIG)
#error FPI_HOSTBRIDGE_COMMAND_ZERO_requires_READONLY_CONFIG
#endif
#ifdef FPI_HOSTBRIDGE_ENUM_NO_RESOURCES
/* Explicit per-byte conventional type0 classification. All implemented
 * writable masks are zero; no error flags or resources exist to latch. */
static bool ignored_header_byte(unsigned off){
 if(off<4)return true; /* vendor/device RO */
 if(off<6)return true; /* command supported mask0: no IO/MEM/BME capability */
 if(off<8)return true; /* status W1C mask0: no latched errors */
 if(off<12)return true; /* revision/class RO */
 if(off<14)return true; /* cacheline/latency unimplemented */
 if(off==14)return true; /* header type RO */
 if(off==15)return true; /* BIST unimplemented */
 if(off<40)return true; /* six unimplemented BARs, fixed zero */
 if(off<48)return true; /* CIS/subsystem RO */
 if(off<52)return true; /* expansion ROM unimplemented */
 if(off<60)return true; /* capability/reserved RO */
 if(off==60)return true; /* interrupt line unimplemented */
 return off<64; /* interrupt pin/minGnt/maxLat RO */
}
#endif
bool fpi_io(struct fpi_state*s,uint16_t port,unsigned width,bool input,uint64_t*rax){
 if(!s||!rax||(width!=1&&width!=2&&width!=4))return false;
 uint32_t v=0;uint64_t mask=width==4?UINT64_C(0xffffffff):(UINT64_C(1)<<(width*8))-1;
 if(port==0xcf8){
  if(width!=4)return false;
  if(input)v=s->address;else{s->address=(uint32_t)*rax&0x80fffffcu;return true;}
 }else{
  if(port<0xcfc||port>0xcff||(port&(width-1))||width>0xd00u-port)return false;
  /* Config mechanism 1: enable, bus/device/function, DWORD register. */
  if(!(s->address&0x80000000u)||(s->address&0x00ffff00u)){
   if(!input)return true; /* No target decodes this configuration cycle. */
   v=(uint32_t)mask;
  }else{
   unsigned off=(s->address&0xfcu)+(port-0xcfcu);
#ifdef FPI_HOSTBRIDGE_ENUM_NO_RESOURCES
   if(!input){
    if(off>=64||width>64-off)return false;
    for(unsigned i=0;i<width;i++)if(!ignored_header_byte(off+i))return false;
    return true;
   }
#endif
   /* Read-only synthetic i440fx-compatible discovery header, no BARs yet.
    * Command=0 disables IO/memory/bus-master decode; no writable capability. */
#ifdef FPI_HOSTBRIDGE_COMMAND_ZERO
   /* Exact idempotent command bytes only: never status or any enable bit. */
   if(!input&&off>=4&&off<6&&width<=6-off&&(*rax&mask)==0)return true;
#endif
   static const unsigned char header[16]={0x86,0x80,0x37,0x12,0,0,0,0,2,0,0,6,0,0,0,0};
#ifdef FPI_HOSTBRIDGE_READONLY_CONFIG
   /* Explicit Syle type0 discovery view, not complete physical i440fx. */
   if(input&&off>=sizeof header&&off<256&&width<=256-off){
    *rax=width==4?0:(*rax&~mask);return true;
   }
#endif
#ifdef FPI_HOSTBRIDGE_NO_BARS
   /* Type0 header six absent BARs, fixed zero, discovery reads only. */
   if(input&&off>=0x10&&off<0x28&&width<=0x28-off){
    *rax=width==4?0:(*rax&~mask);return true;
   }
#endif
   if(!input||off>=sizeof header||width>sizeof header-off)return false;
   for(unsigned i=0;i<width;i++)v|=(uint32_t)header[off+i]<<(8*i);
  }
 }
 *rax=width==4?v:(*rax&~mask)|v;return true;
}

bool fpi_exit(struct fpi_state*s,uint64_t q,uint64_t*rax){
 unsigned size=q&7,width=size==3?4:size==1?2:size==0?1:0;
 if(!width||(q&UINT64_C(0xffffffff0000ffb0)))return false;
 return fpi_io(s,(uint16_t)(q>>16),width,!!(q&8),rax);
}
