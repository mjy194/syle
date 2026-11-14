#define _POSIX_C_SOURCE 200112L
#include "../firmware_mmio_memory.h"
#include "../guest_mmio_mov.h"
#include "../firmware_dirty_cas.h"
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





int main(void){struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t b[15];assert(fmm_fetch(&f,0xfffffff0,b)==15);f.cr4|=8;
#ifdef SYLE_FMM_DE_CONFIG
 assert(fmm_fetch(&f,0xfffffff0,b)==15);f.cr4|=0x2000;assert(fmm_fetch(&f,0xfffffff0,b)==15);
#else
 assert(fmm_fetch(&f,0xfffffff0,b)==0);
#endif
 f.cr4|=0x1000;assert(fmm_fetch(&f,0xfffffff0,b)==0);f.cr4=0x648;assert(fmm_fetch(&f,0xfffffff0,b)==0);
 assert(fwm_release(m));free(h->bytes);free(h);free(m);free(code);free(vars);puts("firmware_mmio_memory_de: PASS (compile-profile mask)");}
