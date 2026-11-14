#ifndef SYLE_HOST_CLOCK_CPUID_DIAG_H
#define SYLE_HOST_CLOCK_CPUID_DIAG_H
#include <stdint.h>
extern void log_text(const char*);
extern void log_hex(uint64_t);
static void hccd_leaf(uint32_t leaf,uint32_t*r){
 __asm__ volatile("cpuid":"=a"(r[0]),"=b"(r[1]),"=c"(r[2]),"=d"(r[3]):"a"(leaf),"c"(0));
}
static void host_clock_cpuid_diag(void){
 uint32_t r[4];hccd_leaf(0,r);uint32_t basic=r[0];hccd_leaf(0x80000000,r);uint32_t ext=r[0];
 log_text("HOST.CLOCK_CPUID max_basic=");log_hex(basic);log_text(" max_extended=");log_hex(ext);log_text("\n");
 const uint32_t leaves[3]={0x15,0x16,0x80000007};
 for(unsigned i=0;i<3;i++){
  uint32_t leaf=leaves[i];if((leaf<0x80000000?basic:ext)<leaf)continue;
  hccd_leaf(leaf,r);log_text("HOST.CLOCK_CPUID leaf=");log_hex(leaf);
  log_text(" eax=");log_hex(r[0]);log_text(" ebx=");log_hex(r[1]);log_text(" ecx=");log_hex(r[2]);log_text(" edx=");log_hex(r[3]);log_text("\n");
 }
}
#endif
