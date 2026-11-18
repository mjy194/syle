#include "ata_pio.h"
#ifndef SYLE_PERSISTENT_KVM_FIXTURE
#error "Scoped persistent KVM fixture required"
#endif
extern bool fwb_kvm_environment(void);
static bool ready,failed;static uint64_t sectors;
static uint8_t in8(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p):"memory");return v;}
static uint16_t in16(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p):"memory");return v;}
static void out8(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p):"memory");}
static void out16(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p):"memory");}
static uint32_t pci_read(unsigned off){uint32_t v,a=0x80002800|off;__asm__ volatile("outl %0,%1"::"a"(a),"Nd"((uint16_t)0xcf8):"memory");__asm__ volatile("inl %1,%0":"=a"(v):"Nd"((uint16_t)0xcfc):"memory");return v;}
static void pci_write(unsigned off,uint32_t v){uint32_t a=0x80002800|off;__asm__ volatile("outl %0,%1"::"a"(a),"Nd"((uint16_t)0xcf8):"memory");__asm__ volatile("outl %0,%1"::"a"(v),"Nd"((uint16_t)0xcfc):"memory");}
static bool bad(void){failed=true;return false;}
static void delay(void){for(unsigned i=0;i<4;i++)(void)in8(0x3f6);}
static bool wait(bool drq){
 for(unsigned i=0;i<1000000;i++){
  uint8_t s=in8(0x1f7);if(!s||s==255)return bad();
  if(s&128)continue;
  if(s&0x21)return bad();
  if(drq?(s&8):!(s&8))return true;
 }
 return bad();
}
static bool command(uint64_t lba,uint8_t cmd){
 if(!ready||failed||lba>=sectors||lba>=0x10000000||!wait(false))return false;
 out8(0x1f6,0xe0|(uint8_t)(lba>>24));delay();
 out8(0x1f2,1);out8(0x1f3,(uint8_t)lba);out8(0x1f4,(uint8_t)(lba>>8));out8(0x1f5,(uint8_t)(lba>>16));out8(0x1f7,cmd);delay();return wait(true);
}
static bool read_sector(void*x,uint64_t lba,uint8_t b[512]){
 (void)x;if(!b||!command(lba,0x20))return bad();
 for(unsigned i=0;i<256;i++){uint16_t v=in16(0x1f0);b[2*i]=(uint8_t)v;b[2*i+1]=(uint8_t)(v>>8);}delay();return wait(false);
}
static bool write_sector(void*x,uint64_t lba,const uint8_t b[512]){
 (void)x;if(!b||!command(lba,0x30))return bad();
 for(unsigned i=0;i<256;i++){out16(0x1f0,(uint16_t)b[2*i]|((uint16_t)b[2*i+1]<<8));}
 delay();return wait(false);
}
static bool flush(void*x){(void)x;if(!ready||failed||!wait(false))return bad();out8(0x1f7,0xe7);delay();return wait(false);}
bool syle_ata_bind(struct vrw_store*out){
 if(!out||ready||failed||!fwb_kvm_environment()||pci_read(0)!=0x70108086||(pci_read(8)&0xffffff00)!=0x01018000)return bad();
 uint32_t c=pci_read(4)&65535;pci_write(4,(c&~4u)|0x401);if((pci_read(4)&0x405)!=0x401)return bad();
 uint32_t timing=pci_read(0x40);pci_write(0x40,timing|0x80008000u);if((pci_read(0x40)&0x80008000u)!=0x80008000u)return bad();
 out8(0x3f6,2);out8(0x1f6,0xa0);delay();out8(0x1f2,0);out8(0x1f3,0);out8(0x1f4,0);out8(0x1f5,0);out8(0x1f7,0xec);delay();if(!wait(true))return false;
 uint16_t id[256];for(unsigned i=0;i<256;i++)id[i]=in16(0x1f0);delay();if(!wait(false))return false;
 static const char serial[21]="SYLE-PERSIST-0001   ";
 for(unsigned i=0;i<20;i++){uint8_t v=(uint8_t)(id[10+i/2]>>((i&1)?0:8));if(v!=(uint8_t)serial[i])return bad();}
 sectors=(uint64_t)id[60]|((uint64_t)id[61]<<16);
 if(sectors!=131072||!(id[49]&512)||(id[83]&0xc000)!=0x4000||!(id[83]&0x1000))return bad();
 ready=true;*out=(struct vrw_store){0,sectors,read_sector,write_sector,flush};return true;
}

bool syle_ata_close(void){
 if(failed)return false;
 if(!ready)return true;
 if(!flush(0))return false;
 ready=false;sectors=0;return true;
}
