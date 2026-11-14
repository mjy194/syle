#define _POSIX_C_SOURCE 200112L
#include "../firmware_fwcfg_target.h"
#include "../guest_mmio_mov.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COUNT (FWM_RAM_PAGES+FWM_FLASH_PAGES+FWM_TABLE_PAGES)
#define BASE UINT64_C(0x100000000)
struct heap {unsigned char*bytes;bool used[COUNT];unsigned allocs,live,maps,wbs,releases;
 unsigned fail_alloc,fail_map,fail_wb,fail_free,duplicate_at,bad_at,unaligned_map;
 uint64_t bad_value;};
static unsigned idx(uint64_t p){assert(p>=BASE&&p<BASE+(uint64_t)COUNT*4096&&!(p&4095));return (unsigned)((p-BASE)/4096);}
static uint64_t alloc(void*x){struct heap*h=x;unsigned n=++h->allocs;
 if(n==h->fail_alloc)return 0;
 if(n==h->duplicate_at)return BASE;
 if(n==h->bad_at)return h->bad_value;
 assert(n<=COUNT&&!h->used[n-1]);h->used[n-1]=true;h->live++;return BASE+(uint64_t)(n-1)*4096;
}
static bool release(void*x,uint64_t p){struct heap*h=x;unsigned i=idx(p);assert(h->used[i]);h->releases++;if(i+1==h->fail_free)return false;h->used[i]=false;h->live--;return true;}
static void*map(void*x,uint64_t p){struct heap*h=x;unsigned i=idx(p);assert(h->used[i]);h->maps++;
 if(h->maps==h->fail_map)return 0;
 return h->bytes+(uint64_t)i*4096+(h->maps==h->unaligned_map?1:0);
}
static bool wb(void*x,uint64_t p){struct heap*h=x;assert(h->used[idx(p)]);return ++h->wbs!=h->fail_wb;}

static void put(struct firmware_memory*m,uint64_t a,uint64_t v){assert(fwm_write_ram(m,a,&v,8));}

static void no(struct fmm_memory*f,uint64_t va,unsigned count){struct fft_plan p={.bytes=(uint8_t*)(uintptr_t)0x1234,.gpa=0x5678,.count=9},old=p;assert(!fft_prepare(f,va,count,0x10000,&p)&&!memcmp(&p,&old,sizeof p));}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536]={0},vars[4096]={0};struct sg_memory_ops o={h,alloc,release,map,wb};
 assert(fwm_prepare(m,&o,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0x4023);put(m,0x4000,0x8063);put(m,0x4000+16*8,0x10063);
 struct fft_plan p={0};assert(fft_prepare(&f,0x40,64,0x10000,&p)&&p.gpa==0x8040&&p.count==64&&p.bytes==h->bytes+(m->ram[8]-BASE)+0x40);
 for(unsigned i=0;i<64;i++)assert(!p.bytes[i]); /* helper did not modify target */
 no(&f,0,0);no(&f,0,65);no(&f,4095,2);no(&f,UINT64_MAX,2);no(&f,0x800000000000ULL,1);
 assert(fft_prepare(&f,4095,1,0x10000,&p)&&p.gpa==0x8fff);
 /* Every active page-table level is excluded, both GPA and HPA alias. */
 for(unsigned i=1;i<=4;i++){put(m,0x4000,(uint64_t)i*4096+0x63);no(&f,0,1);}put(m,0x4000,0x8063);
 uint64_t saved=m->ram[8];m->ram[8]=m->ram[1];no(&f,0,1);m->ram[8]=saved;
 /* Last low-RAM page valid, higher GPA and all flash/MMIO reject. */
 put(m,0x4000,(FWM_RAM_SIZE-4096)|0x63);assert(fft_prepare(&f,4096-64,64,0x10000,&p)&&p.gpa==FWM_RAM_SIZE-64);
 uint64_t rejected[]={FWM_RAM_SIZE,m->layout.code.base,m->layout.vars.base,0xfee00000};
 for(unsigned i=0;i<4;i++){put(m,0x4000,rejected[i]|0x63);no(&f,0,8);}put(m,0x4000,0x8063);
 /* Present, accessed, dirty, WP permissions, NX and upper physical bits. */
 uint64_t invalid[]={0x8062,0x8043,0x8023,0x8061,0x8063ULL|(1ULL<<63),0x8063ULL|(1ULL<<32)};
 for(unsigned i=0;i<6;i++){put(m,0x4000,invalid[i]);no(&f,0,8);}put(m,0x4000,0x8063);
 put(m,0x1000,0x2021);no(&f,0,8);put(m,0x1000,0x2023);
 /* Missing ownership fails before that token can reach map. */
 saved=m->ram[8];m->ram[8]=0xdead000;no(&f,0,8);m->ram[8]=saved;
 /* Fail walk maps and final target mapping; intermediate optional fetch
  * prefix failures may legitimately retain the required two bytes. */
 unsigned before=h->maps;assert(fft_prepare(&f,0,1,0x10000,&p));unsigned total=h->maps-before;
 for(unsigned i=1;i<=8;i++){h->fail_map=h->maps+i;no(&f,0,1);h->fail_map=0;}
 h->fail_map=h->maps+total;no(&f,0,1);h->fail_map=0;
 h->unaligned_map=h->maps+total;no(&f,0,1);h->unaligned_map=0;
 assert(!fft_prepare(&f,0,1,0x10000,0));no(0,0,1);
 /* Source instruction page itself must not be overwritten. */
 put(m,0x4000,0x10063);no(&f,0,1);put(m,0x4000,0x8063);
 saved=m->ram[8];m->ram[8]=m->ram[16];no(&f,0,1);m->ram[8]=saved;
 /* Separate source branch has its own PD/PT, both excluded. */
 put(m,0x2000+8,0x5023);put(m,0x5000,0x6023);put(m,0x6000,0x10063);
 for(unsigned i=5;i<=6;i++){put(m,0x4000,(uint64_t)i*4096+0x63);assert(!fft_prepare(&f,0,1,0x40000000,&p));}put(m,0x4000,0x8063);
 assert(fft_prepare(&f,0,1,0x40000000,&p));
 /* Second instruction byte crosses to a new leaf: reject source alias there. */
 put(m,0x6000+8,0x8063);assert(!fft_prepare(&f,0,1,0x40000fff,&p));
 put(m,0x6000+8,0x11063);assert(fft_prepare(&f,0,1,0x40000fff,&p));
 /* 2MiB leaf: root, PDPT and PD excluded even without a PT. */
 put(m,0x3000,0xe3);assert(fft_prepare(&f,0x8000,64,0x10000,&p)&&p.gpa==0x8000);
 no(&f,0x1000,1);no(&f,0x2000,1);no(&f,0x3000,1);
 assert(fwm_release(m)&&!h->live);no(&f,0,1);free(h->bytes);free(h);free(m);
 puts("fwcfg target: owned writable single-page plan, table alias, bounds and map failures PASS");return 0;
}
