#include "firmware_fwcfg.h"
#include <stddef.h>
/* Explicit wire bytes, independent of host endianness. File directory is a
 * BE32 zero count, not an invented empty e820 memory map. */
static const uint8_t signature[]={0x51,0x45,0x4d,0x55};
static const uint8_t revision[]={1,0,0,0}; /* DMA bit1 absent */
static const uint8_t ram_size[]={0,0,0,8,0,0,0,0}; /* LE64 128MiB */
static const uint8_t cpus[]={1,0};
static const uint8_t directory[]={0,0,0,0};
#ifdef FFC_BOOT_MENU_DEFAULT
static const uint8_t boot_menu_default[]={0,0}; /* LE16: use firmware policy */
#endif
#ifdef FFC_NO_KERNEL_BLOBS
/* No externally supplied setup/kernel/initrd/cmdline payload in this profile. */
static const uint8_t no_blob_size[]={0,0,0,0}; /* LE32 */
#endif
static const uint8_t*item(uint16_t selector,size_t*n){
 switch(selector){
 case 0:*n=sizeof signature;return signature;
 case 1:*n=sizeof revision;return revision;
 case 3:*n=sizeof ram_size;return ram_size;
 case 5:case 0xf:*n=sizeof cpus;return cpus;
 case 0x19:*n=sizeof directory;return directory;
#ifdef FFC_BOOT_MENU_DEFAULT
 case 0x0e:*n=sizeof boot_menu_default;return boot_menu_default;
#endif
#ifdef FFC_NO_KERNEL_BLOBS
 case 0x17:case 8:case 0xb:case 0x14:*n=sizeof no_blob_size;return no_blob_size;
#endif
 default:return 0;
 }
}
bool ffc_init(struct ffc_state*s,uint64_t ram_size,unsigned cpu_count){
 if(!s||s->ready||s->cursor||s->selector||ram_size!=UINT64_C(128)*1024*1024||cpu_count!=1)return false;
 *s=(struct ffc_state){.ready=true};return true;
}
bool ffc_io(struct ffc_state*s,uint16_t port,unsigned width,bool input,uint64_t*rax){
 if(!s||!s->ready||!rax)return false;
 size_t n=0;
 if(port==0x510){
  uint16_t key=(uint16_t)*rax;
  if(input||width!=2||!item(key,&n))return false;
  s->selector=key;s->cursor=0;return true;
 }
 if(port!=0x511||!input||width!=1||s->cursor==UINT64_MAX)return false;
 const uint8_t*p=item(s->selector,&n);if(!p)return false;
 uint8_t value=s->cursor<n?p[s->cursor]:0;
 s->cursor++;*rax=(*rax&~UINT64_C(255))|value;return true;
}
bool ffc_exit(struct ffc_state*s,uint64_t q,uint64_t*rax){
 unsigned size=q&7,width=size==3?4:size==1?2:size==0?1:0;
 if(!width||(q&UINT64_C(0xffffffff0000ffb0)))return false;
 return ffc_io(s,(uint16_t)(q>>16),width,!!(q&8),rax);
}
