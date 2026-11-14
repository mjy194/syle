#include "transport.h"
#define RO (1u<<5)
static bool bad(struct vrt *s){s->failed=true;return false;}
static uint8_t cfg(struct vrt *s,unsigned o){
 uint32_t v=0;unsigned start=o&~3u;
 switch(start){
 case 0:v=0x10011af4;break;
 case 4:v=s->command;break;
 case 8:v=0x01000000;break; /* mass storage / SCSI, revision zero */
 case 16:v=s->probe?0xffffffc1u:s->bar|1u;break;
 case 44:v=0x00021af4;break;
 /* Polling profile: no usable interrupt line/pin, no capabilities. */
 default:break;
 }
 return (uint8_t)(v>>((o&3)*8));
}
bool vrt_init(struct vrt *s,struct vro_io io,const uint8_t *disk,size_t size){
 if(!s)return false;
 *s=(struct vrt){.io=io,.disk=disk,.disk_size=size};
 if(!disk||!size||size%512||!io.admit||!io.read||!io.write||!io.release)return bad(s);
 return true;
}
bool vrt_config(struct vrt *s,unsigned off,unsigned n,bool write,uint32_t *v){
 if(!s||s->failed)return false;
 if(!v||(n!=1&&n!=2&&n!=4)||off>=256||n>256-off||off%n)return bad(s);
 if(!write){*v=0;for(unsigned i=0;i<n;i++)*v|=(uint32_t)cfg(s,off+i)<<(8*i);return true;}
 if(off>=16&&off<20){
  /* Allocation/probe via aligned DWORD only, fail unsupported partial BAR
   * writes explicitly instead of claiming complete PCI configuration. */
  if(off!=16||n!=4)return bad(s);
  if(*v==UINT32_MAX){s->probe=true;return true;}
  uint32_t b=*v&0xffffffc0u;
  if(b>0xffc0u)return bad(s);
  s->bar=b;s->probe=false;return true;
 }
 for(unsigned i=0;i<n;i++){
  unsigned o=off+i;uint8_t b=(uint8_t)(*v>>(8*i));
  if(o==4)s->command=(s->command&0xff00u)|(b&5u);
  if(o==5)s->command=(s->command&0xffu)|((uint16_t)(b&4u)<<8);
 }
 /* Remaining registers are read-only or unimplemented, writes ignored. */
 return true;
}
static void reset(struct vrt *s){
 s->features=0;s->pfn=0;s->select=0;s->status=0;s->started=false;
 s->backend=(struct vro){0};
}
bool vrt_io(struct vrt *s,uint16_t port,unsigned n,bool write,uint32_t *v){
 if(!s||s->failed)return false;
 if(!v||!s->bar||s->probe||!(s->command&1)||port<s->bar||(uint32_t)port-s->bar>=64)return bad(s);
 unsigned o=port-s->bar;
 if((o==0||o==4||o==8)?n!=4:(o==12||o==14||o==16)?n!=2:(o==18||o==19)?n!=1:!(o>=20&&o<28&&(n==1||n==2||n==4)&&n<=28-o))return bad(s);
 if(write&&n<4)*v&=(1u<<(8*n))-1;
 if(!write){switch(o){
  case 0:*v=RO;break;
  case 4:*v=s->features;break;
  case 8:*v=s->select?0:s->pfn;break;
  case 12:*v=s->select?0:128;break;
  case 14:*v=s->select;break;
  case 16:return bad(s); /* notify is write-only in this subset */
  case 18:*v=s->status;break;
  case 19:*v=0;break; /* no interrupt generated in polling profile */
  default:*v=(uint32_t)(((uint64_t)s->disk_size/512)>>(8*(o-20)));if(n<4)*v&=(1u<<(8*n))-1;break;
 }return true;}
 if(o==18){
  uint8_t x=(uint8_t)*v;
  if(!x){reset(s);return true;}
  if((x&~0x87u)||(s->status&~x)||(!(x&1))||((x&4)&&!(x&2)))return bad(s);
  if((x&4)&&(!s->pfn||s->backend.failed))return bad(s);
  s->status=x;return true;
 }
 if(s->status&0x80)return bad(s);
 switch(o){
 case 4:
  if(!(s->status&2)||(s->status&4)||s->started||(*v&~RO))return bad(s);
  s->features=*v;return true;
 case 8:
  if(s->select||!(s->status&2)||(s->status&4)||s->started||s->pfn||!*v)return bad(s);
  if(!vro_init(&s->backend,s->io,s->disk,s->disk_size,*v))return bad(s);
  s->pfn=*v;return true;
 case 14:s->select=(uint16_t)*v;return true;
 case 16:
  if(*v||!(s->status&2)||!(s->command&4)||!s->pfn)return bad(s);
  s->started=true; /* Legacy first use finalizes features, even without a write. */
  if(vro_poll(&s->backend)<0)return bad(s);
  return true;
 default:return bad(s);
 }
}
