#include "../linux_mmio_memory.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t ram[0x10000];static unsigned reads;static bool failread;
static bool readram(void*c,uint64_t p,void*out,size_t n){(void)c;assert(p<sizeof ram&&n<=sizeof ram-p);reads++;if(failread)return false;memcpy(out,ram+p,n);return true;}
static void put(uint64_t a,uint64_t v){for(unsigned i=0;i<8;i++)ram[a+i]=(uint8_t)(v>>(i*8));}
static struct lmm_memory m;
static void setup(void){memset(ram,0,sizeof ram);reads=0;failread=false;m=(struct lmm_memory){.read=readram,.ram_size=sizeof ram,.cr0=0x80010033,.cr3=0x1018,.cr4=0x2020,.efer=0xd01,.cs_selector=0x10,.cs_ar=0xa09b};put(0x1000,0x2023);put(0x2000,0x3023);put(0x3000,0x4023);put(0x4000,0x8063);put(0x4008,0xfee00063);memset(ram+0x8000,0x90,4096);}
int main(void){uint64_t p=0;uint8_t b[15];setup();assert(lmm_translate(&m,0,LMM_EXEC,&p)&&p==0x8000);assert(lmm_fetch(&m,0,b)==15&&b[0]==0x90);
 assert(lmm_validate(&m,0x1010,4,true,48,0x182,0x1010,0xfee00010,0,0xfee00000,4096));
 assert(lmm_validate(&m,0x1010,4,false,48,0x181,0x1010,0xfee00010,0,0xfee00000,4096));
 memset(b,0xcc,15);assert(lmm_fetch(&m,4095,b)==1&&b[0]==0x90&&b[1]==0xcc);
 put(0x4008,0x9063);ram[0x8fff]=0x8b;ram[0x9000]=0;assert(lmm_fetch(&m,4095,b)==15&&b[0]==0x8b&&b[1]==0);
 put(0x4008,0x8000000000009063ull);assert(lmm_fetch(&m,4095,b)==1);
 setup();put(0x4008,0x80000000fee00063ull);assert(lmm_translate(&m,0x1000,LMM_READ,&p));m.efer&=~0x800ull;assert(!lmm_translate(&m,0x1000,LMM_READ,&p));
 setup();put(0x4008,0xfee00061);assert(!lmm_translate(&m,0x1000,LMM_WRITE,&p));m.cr0&=~(1ull<<16);assert(lmm_translate(&m,0x1000,LMM_WRITE,&p));
 setup();put(0x4008,0xfee00023);assert(!lmm_translate(&m,0x1000,LMM_WRITE,&p));
 const uint64_t entries[]={0x1000,0x2000,0x3000,0x4000};
 for(unsigned i=0;i<4;i++){setup();put(entries[i],0);assert(!lmm_translate(&m,0,LMM_READ,&p));setup();put(entries[i],0x100000023ull);assert(!lmm_translate(&m,0,LMM_READ,&p));setup();put(entries[i],0x8003);assert(!lmm_translate(&m,0,LMM_READ,&p));}
 setup();put(0x3000,0xfee000e3);assert(lmm_translate(&m,0x1234,LMM_READ,&p)&&p==0xfee01234);assert(!lmm_fetch(&m,0,b));put(0x3000,0xfee020e3);assert(!lmm_translate(&m,0,LMM_READ,&p));
 setup();put(0x2000,0xe3);assert(!lmm_translate(&m,0,LMM_READ,&p));
 setup();put(0x1000,0xff000023);assert(!lmm_translate(&m,0,LMM_READ,&p));
 setup();assert(!lmm_translate(&m,0x800000000000ull,LMM_READ,&p));assert(!lmm_fetch(&m,UINT64_MAX,b));
 const unsigned forbidden[]={12,17,20,21,22,23};for(unsigned i=0;i<6;i++){setup();m.cr4|=1ull<<forbidden[i];assert(!lmm_translate(&m,0,LMM_READ,&p));}
 setup();m.cs_ar=0xc09b;assert(!lmm_translate(&m,0,LMM_READ,&p));setup();m.cs_selector=0x13;assert(!lmm_translate(&m,0,LMM_READ,&p));setup();m.cs_base=4096;assert(!lmm_translate(&m,0,LMM_READ,&p));
 setup();m.cr3|=1;assert(!lmm_translate(&m,0,LMM_READ,&p));setup();m.efer&=~0x400ull;assert(!lmm_translate(&m,0,LMM_READ,&p));
 setup();failread=true;p=0xdead;assert(!lmm_translate(&m,0,LMM_READ,&p)&&p==0xdead);
 setup();assert(!lmm_validate(&m,0,4,false,48,0x181,0,0x8000,0,0x8000,4096));
 setup();for(unsigned bit=0;bit<16;bit++)assert(!lmm_validate(&m,0x1010,4,true,48,0x182^(1ull<<bit),0x1010,0xfee00010,0,0xfee00000,4096));
 assert(!lmm_validate(&m,0x1010,4,true,48,0x182,0x1011,0xfee00010,0,0xfee00000,4096));
 assert(!lmm_validate(&m,0x1010,4,true,48,0x182,0x1010,0xfee00011,0,0xfee00000,4096));
 assert(!lmm_validate(&m,0x1fff,4,true,48,0x182,0x1fff,0xfee00fff,0,0xfee00000,8192));
 assert(!lmm_validate(&m,0x1010,4,true,48,0x182,0x1010,0xfee00010,0x8000000e,0xfee00000,4096));
 assert(!lmm_validate(&m,0x1010,4,true,0x80000030,0x182,0x1010,0xfee00010,0,0xfee00000,4096));
 assert(!lmm_validate(&m,0x1010,4,true,48,0x182,0x1010,0xfee00010,0,UINT64_MAX,2));
 puts("linux_mmio_memory: private RAM fetch, 4K/2M translation, permissions and EPT evidence PASS (host only)");
}
