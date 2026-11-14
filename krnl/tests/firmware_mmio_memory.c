#define _POSIX_C_SOURCE 200112L
#include "../firmware_mmio_memory.h"
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
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536],vars[4096];memset(code,0x90,sizeof code);memset(vars,0xcc,sizeof vars);
 struct sg_memory_ops o={h,alloc,release,map,wb};
 assert(fwm_prepare(m,&o,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={m,0x80000033,0x1000,0x2660,0x500,0,0xa09b,8,false};
 /* Identity low RAM, upper ROM and APIC through distinct owned low-RAM PTs. */
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x2018,0x4023);
 put(m,0x3000,0xe3); /* 2MiB RAM RWAD */
 put(m,0x4000+511*8,0x5023);
 for(unsigned i=0;i<16;i++)put(m,0x5000+(496+i)*8,0xffff0000ULL+(uint64_t)i*4096+0x63);
 put(m,0x4000+503*8,0x6023);put(m,0x6000,0xfee00063);
 uint64_t p=0xdead;uint8_t b[15];
 /* Decode from ROM bytes, not an exit-supplied instruction length. */
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE);
 rom[0xff0]=0x8b;rom[0xff1]=0x00;
 uint64_t regs[16]={0xfee00020};struct guest_mmio_mov mov;
 assert(guest_mmio_mov_decode(b,fmm_fetch(&f,0xfffffff0,b),0xfffffff0,regs,&mov));
 assert(mov.width==4&&!mov.write&&mov.linear==0xfee00020&&mov.length==2);
 rom[0xff0]=rom[0xff1]=0x90;
 assert(fmm_translate(&f,0xfffffff0,FMM_EXEC,&p)&&p==0xfffffff0);
 assert(fmm_fetch(&f,0xfffffff0,b)==15&&b[0]==0x90);
 assert(fmm_fetch(&f,0xfffffff9,b)==7); /* physical32 top, no wrap */
 assert(fmm_fetch(&f,0xffff0ffa,b)==15); /* ROM cross-page */
 assert(fmm_fetch(&f,0x1ffffa,b)==6); /* stops at absent next PDE */
 assert(fmm_validate(&f,0xfee00020,4,false,48,0x181,0xfee00020,0xfee00020,0,0xfee00000,4096));
 assert(fmm_validate(&f,0xfee00020,4,true,48,0x182,0xfee00020,0xfee00020,0,0xfee00000,4096));
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x581,0xfee00020,0xfee00020,0,0xfee00000,4096));
 f.advanced_ept_info=true;
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x181,0xfee00020,0xfee00020,0,0xfee00000,4096));
 assert(fmm_validate(&f,0xfee00020,4,false,48,0x581,0xfee00020,0xfee00020,0,0xfee00000,4096));
 assert(fmm_validate(&f,0xfee00020,4,true,48,0x582,0xfee00020,0xfee00020,0,0xfee00000,4096));
 put(m,0x6000,0xfee00061); /* RO leaf conflicts with reported RW, even WP0. */
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x581,0xfee00020,0xfee00020,0,0xfee00000,4096));
 assert(!fmm_validate(&f,0xfee00020,4,true,48,0x582,0xfee00020,0xfee00020,0,0xfee00000,4096));
 assert(fmm_validate(&f,0xfee00020,4,false,48,0x181,0xfee00020,0xfee00020,0,0xfee00000,4096));
 put(m,0x1000,0x2027);put(m,0x2018,0x4027);put(m,0x4000+503*8,0x6027);put(m,0x6000,0xfee00067);
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x581,0xfee00020,0xfee00020,0,0xfee00000,4096));
 put(m,0x1000,0x2023);put(m,0x4000+503*8,0x6023);
 put(m,0x6000,0xfee00063);put(m,0x2018,0x4021); /* Upper-level RW also matters. */
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x581,0xfee00020,0xfee00020,0,0xfee00000,4096));
 put(m,0x2018,0x4023);
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x781,0xfee00020,0xfee00020,0,0xfee00000,4096));
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0xd81,0xfee00020,0xfee00020,0,0xfee00000,4096));
 f.advanced_ept_info=false;
 assert(!fmm_fetch(&f,0xfee00000,b));
 /* A translated VARS page is not an instruction source. */
 put(m,0x5000+495*8,m->layout.vars.base|0x63);assert(!fmm_fetch(&f,m->layout.vars.base,b));
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x181,0xfee00021,0xfee00020,0,0xfee00000,4096));
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x181,0xfee00020,0xfee00024,0,0xfee00000,4096));
 assert(!fmm_validate(&f,0xfee00fff,4,false,48,0x181,0xfee00fff,0xfee00fff,0,0xfee00000,4096));
 assert(!fmm_validate(&f,0xfee00020,4,false,48,0x181,0xfee00020,0xfee00020,1,0xfee00000,4096));
 assert(!fmm_validate(&f,0xffff0000,4,false,48,0x181,0xffff0000,0xffff0000,0,0xffff0000,4096));
 /* Mapping a high canonical VA works; noncanonical inputs fail unchanged. */
 put(m,0x1000+511*8,0x2023);assert(fmm_translate(&f,0xffffff8000000123ULL,FMM_READ,&p)&&p==0x123);
 p=0xdead;assert(!fmm_translate(&f,0x800000000000ULL,FMM_READ,&p)&&p==0xdead);
 uint64_t bad[]={0x2003,0x2023ULL|(1ULL<<32),0x2023ULL|(1ULL<<63),0x20a3,m->layout.code.base|0x23,0xfee00023};
 for(unsigned i=0;i<sizeof bad/sizeof*bad;i++){put(m,0x1000,bad[i]);assert(!fmm_translate(&f,0,FMM_READ,&p));}put(m,0x1000,0x2023);
 put(m,0x2000,0xe3);assert(!fmm_translate(&f,0,FMM_READ,&p));put(m,0x2000,0x3023); /* 1GiB reject */
 put(m,0x3000,0x20e3);assert(!fmm_translate(&f,0,FMM_READ,&p)); /* reserved 2M */
 put(m,0x3000,0xa3);assert(!fmm_translate(&f,0,FMM_WRITE,&p)); /* D missing */
 put(m,0x3000,0xe1);f.cr0|=0x10000;assert(!fmm_translate(&f,0,FMM_WRITE,&p));f.cr0&=~0x10000ULL;
 assert(fmm_translate(&f,0,FMM_WRITE,&p));put(m,0x3000,0xe3); /* WP0 supervisor semantics */
 /* Ownership failures are rejected before map callbacks; no host PA exposure. */
 uint64_t saved=m->ram[1];unsigned maps=h->maps;m->ram[1]=0xfee00000;assert(!fmm_translate(&f,0,FMM_READ,&p)&&h->maps==maps);m->ram[1]=saved;
 unsigned last=m->flash_pages-1;saved=m->flash[last];m->flash[last]=0xffff0000;assert(!fmm_fetch(&f,0xfffffff0,b));m->flash[last]=saved;
 h->fail_map=h->maps+1;assert(!fmm_fetch(&f,0xfffffff0,b));h->fail_map=0;
 f.cr4|=0x1000;assert(!fmm_fetch(&f,0,b));f.cr4=0x2660;
 f.efer=0xd00;assert(!fmm_fetch(&f,0,b));f.efer=0x500;
 f.cs_ar=0xc09b;assert(!fmm_fetch(&f,0,b));f.cs_ar=0xa09b;
 m->quarantined=true;assert(!fmm_fetch(&f,0,b));m->quarantined=false;
 assert(fwm_release(m)&&!h->live);assert(!fmm_fetch(&f,0,b));free(h->bytes);free(h);free(m);
 puts("firmware MMIO memory: owned RAM/ROM fetch, strict walks, MMIO evidence PASS");return 0;
}
