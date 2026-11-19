#include "firmware_fwcfg.h"
#include <stddef.h>
/* Explicit wire bytes, independent of host endianness. File directory is a
 * BE32 zero count, not an invented empty e820 memory map. */
static const uint8_t signature[]={0x51,0x45,0x4d,0x55};
#ifdef FFC_RAMFB
static const uint8_t revision[]={3,0,0,0};
#else
static const uint8_t revision[]={1,0,0,0}; /* DMA bit1 absent */
#endif
static const uint8_t ram_size[]={0,0,0,8,0,0,0,0}; /* LE64 128MiB */
#ifdef FFC_BOOT_MENU_DEFAULT
static const uint8_t boot_menu_default[]={0,0};
#endif
#ifdef FFC_RAMFB
static const uint8_t directory[68]={0,0,0,1,0,0,0,28,0,0x20,0,0,'e','t','c','/','r','a','m','f','b'};
/* OVMF installs checksum-valid ACPI tables referenced by table-loader's
 * ADD_POINTER records. The leading LE64 entries are blob-relative MADT and HPET pointers.
 * The legacy PIC remains the external interrupt controller. */
struct fw_file { uint8_t size_be[4],selector_be[2],reserved[2]; char name[56]; };
static const struct {
 uint8_t count[4];
 struct fw_file files[3];
} dual_directory={{0,0,0,3},{
 {{0,0,0,28},{0,0x20},{0},"etc/ramfb"},
 {{0,0,0,132},{0,0x21},{0},"etc/acpi/tables"},
 {{0,0,1,128},{0,0x22},{0},"etc/table-loader"}
}};
struct loader_allocate { uint8_t type[4];char file[56];uint8_t alignment[4],zone,pad[63]; };
struct loader_pointer { uint8_t type[4];char file[56],target[56];uint8_t offset[4],width,pad[7]; };
static const struct {struct loader_allocate allocate;struct loader_pointer madt,hpet;} dual_loader={
 {{1},"etc/acpi/tables",{16},1,{0}},
 {{2},"etc/acpi/tables","etc/acpi/tables",{0},8,{0}},
 {{2},"etc/acpi/tables","etc/acpi/tables",{8},8,{0}}
};
_Static_assert(sizeof dual_loader==384 && sizeof dual_directory==196,"fw_cfg wire layout");
static const uint8_t dual_tables[132]={
 16,0,0,0,0,0,0,0, 76,0,0,0,0,0,0,0,
 'A','P','I','C',60,0,0,0,5,166,'S','Y','L','E',' ',' ',
 'S','Y','L','E',' ','S','M','P',1,0,0,0,'S','Y','L','E',1,0,0,0,
 0,0,0xe0,0xfe,1,0,0,0,
 0,8,0,0,1,0,0,0, 0,8,1,1,1,0,0,0,
 'H','P','E','T',56,0,0,0,1,21,'S','Y','L','E',' ',' ',
 'S','Y','L','E','H','P','E','T',1,0,0,0,'S','Y','L','E',1,0,0,0,
 1,2,0x86,0x80,0,0,0,0,0,0,0xd0,0xfe,0,0,0,0,0,128,0,0
};
static const uint8_t ramfb[28]={0};
#endif
#ifndef FFC_RAMFB
static const uint8_t directory[]={0,0,0,0};
#endif
#ifdef FFC_NO_KERNEL_BLOBS
/* No externally supplied setup/kernel/initrd/cmdline payload in this profile. */
static const uint8_t no_blob_size[]={0,0,0,0}; /* LE32 */
#endif
static const uint8_t*item(const struct ffc_state *s,uint16_t selector,size_t*n){
#ifdef FFC_RAMFB
 if(s->cpu_count_le[0]==2){
  if(selector==0x19){*n=sizeof dual_directory;return (const uint8_t*)&dual_directory;}
  if(selector==0x21){*n=sizeof dual_tables;return dual_tables;}
  if(selector==0x22){*n=sizeof dual_loader;return (const uint8_t*)&dual_loader;}
 }
#endif
 switch(selector){
 case 0:*n=sizeof signature;return signature;
 case 1:*n=sizeof revision;return revision;
 case 3:*n=sizeof ram_size;return ram_size;
 case 5:case 0xf:*n=sizeof s->cpu_count_le;return s->cpu_count_le;
 case 0x19:*n=sizeof directory;return directory;
#ifdef FFC_RAMFB
 case 0x20:*n=sizeof ramfb;return ramfb;
#endif
#ifdef FFC_BOOT_MENU_DEFAULT
 case 0xe:*n=sizeof boot_menu_default;return boot_menu_default;
#endif
#ifdef FFC_NO_KERNEL_BLOBS
 case 0x17:case 8:case 0xb:case 0x14:*n=sizeof no_blob_size;return no_blob_size;
#endif
 default:return 0;
 }
}
bool ffc_init(struct ffc_state*s,uint64_t ram_size,unsigned cpu_count){
 if(!s||s->ready||s->cursor||s->selector||ram_size!=UINT64_C(128)*1024*1024||(cpu_count!=1&&cpu_count!=2))return false;
 *s=(struct ffc_state){.ready=true,.cpu_count_le={(uint8_t)cpu_count,0}};return true;
}
bool ffc_io(struct ffc_state*s,uint16_t port,unsigned width,bool input,uint64_t*rax){
 if(!s||!s->ready||!rax)return false;
 size_t n=0;
 if(port==0x510){
  uint16_t key=(uint16_t)*rax;
  if(input||width!=2||!item(s,key,&n))return false;
  s->selector=key;s->cursor=0;return true;
 }
 if(port!=0x511||!input||width!=1||s->cursor==UINT64_MAX)return false;
 const uint8_t*p=item(s,s->selector,&n);if(!p)return false;
 uint8_t value=s->cursor<n?p[s->cursor]:0;
 s->cursor++;*rax=(*rax&~UINT64_C(255))|value;return true;
}
bool ffc_exit(struct ffc_state*s,uint64_t q,uint64_t*rax){
 unsigned size=q&7,width=size==3?4:size==1?2:size==0?1:0;
 if(!width||(q&UINT64_C(0xffffffff0000ffb0)))return false;
 return ffc_io(s,(uint16_t)(q>>16),width,!!(q&8),rax);
}
