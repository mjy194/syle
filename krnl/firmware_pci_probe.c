#include "firmware_pci_probe.h"
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
   /* Read-only synthetic i440fx-compatible discovery header, no BARs yet.
    * Command=0 disables IO/memory/bus-master decode; no writable capability. */
   static const unsigned char header[16]={0x86,0x80,0x37,0x12,0,0,0,0,2,0,0,6,0,0,0,0};
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
