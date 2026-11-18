#include "../firmware_display.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct firmware_memory memory;
static uint8_t *ram;
static bool allow=true;
/* The test isolates scanout's caller contract; real ownership/map behavior is
 * separately exercised by firmware_memory and native lifecycle validation. */
bool fwm_owned_page(const struct firmware_memory*m,uint64_t p){return m==&memory&&allow&&p&&!(p&4095);}
bool fwm_read_ram(struct firmware_memory*m,uint64_t a,void*p,size_t n){if(m!=&memory||a>FWM_RAM_SIZE||n>FWM_RAM_SIZE-a)return false;memcpy(p,ram+a,n);return true;}
int main(void){
 ram=calloc(1,FWM_RAM_SIZE);assert(ram);uint32_t*host=calloc(1024*768+2,4);assert(host);host[1024*768]=0xdeadbeef;
 struct syle_framebuffer fb={.base=(uintptr_t)host,.size=1024*768*4,.width=1024,.height=768,.stride=1024,.format=0,.valid=1};
 struct syle_framebuffer bad=fb;bad.size--;assert(!firmware_display_bind(&bad));assert(firmware_display_bind(&fb));assert(!firmware_display_bind(&fb));
 memory.prepared=true;for(unsigned i=0;i<FWM_RAM_PAGES;i++)memory.ram[i]=0x100000+(uint64_t)i*4096;
 struct frfb_state state={.active=true,.config={.address=4096,.width=16,.height=16,.stride=64,.span=1024}};
 for(unsigned i=0;i<256;i++)((uint32_t*)(ram+4096))[i]=0x123456;
 assert(firmware_display_present(&memory,&state,1,false));assert(host[0]==0x563412&&host[15*1024+15]==0x563412&&host[16]==0);
 assert(firmware_display_stats().frames==1);assert(firmware_display_present(&memory,&state,2,false)&&firmware_display_stats().frames==1);
 ((uint32_t*)(ram+4096))[0]=0xabcdef;assert(firmware_display_present(&memory,&state,2,true)&&host[0]==0xefcdab);
 allow=false;assert(!firmware_display_present(&memory,&state,2000000,true));allow=true;
 state.config.address=UINT64_MAX-100;assert(!firmware_display_present(&memory,&state,2000000,true));state.config.address=4096;
 memory.prepared=false;assert(!firmware_display_present(&memory,&state,2000000,true));memory.prepared=true;
 firmware_display_revoke();assert(!firmware_display_stats().active&&host[0]==0&&host[1024*768]==0xdeadbeef);
 firmware_display_reset();assert(firmware_display_stats().frames==0);
 free(host);free(ram);puts("display: destination bounds, RGB/BGR conversion, row stride, cadence, forced frame, owner rejection and revoke PASS");
}
