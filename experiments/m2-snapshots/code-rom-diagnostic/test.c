#define _POSIX_C_SOURCE 200112L
#include "firmware_mmio_memory.h"

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
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);code[16]=0x78;
 struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2668,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x1000,0x2023);put(m,0x2018,0x4023);put(m,0x4000+510*8,0x6023);put(m,0x6000+132*8,0xffc84063);
 struct fmm_code_diagnostic d;
 unsigned char*before=malloc((size_t)COUNT*4096);assert(before);memcpy(before,h->bytes,(size_t)COUNT*4096);
 assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d));
 assert(d.code_backing_ok&&d.byte_ok&&d.original==0x78&&d.walk.read_ok&&d.walk.write_ok&&d.walk.read_gpa==0xffc84010&&d.walk.write_gpa==0xffc84010&&d.walk.write_attributes==0x400&&!d.walk.vars_backing_ok);
 assert(d.walk.count==4&&d.walk.ept_count==4&&d.walk.ept_permissions[3]==0x35);
 assert(!memcmp(before,h->bytes,(size_t)COUNT*4096));
 /* Guest D absence is diagnosed, never repaired. */
 put(m,0x6000+132*8,0xffc84023);assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.walk.write_ok&&d.walk.read_ok&&d.code_backing_ok);put(m,0x6000+132*8,0xffc84063);
 put(m,0x6000+132*8,0xffc84061);assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.walk.write_ok);put(m,0x6000+132*8,0xffc84063);
 /* These diagnostics must not be confused with an execution gate. */
 assert(!fmm_validate_vars_write(&f,0xffc84010,1,48,0x5aa,0xffc84010,0xffc84010,0));
 assert(!fmm_validate(&f,0xffc84010,1,true,48,0x5aa,0xffc84010,0xffc84010,0,0xffc84000,4096));
 uint64_t*leaf=(uint64_t*)(h->bytes+(m->tables[68]-BASE));uint64_t old=leaf[132];
 leaf[132]|=2;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok&&!d.byte_ok);leaf[132]=old;
 old=m->ram[8];m->ram[8]=m->flash[132];assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok);m->ram[8]=old;
 old=m->flash[133];m->flash[133]=m->flash[132];assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok);m->flash[133]=old;
 assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc00010,&d)&&!d.code_backing_ok);
 f.cr3=0xffc84000;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.walk.read_ok&&!d.walk.count);f.cr3=0x1000;
 m->layout.code.write=true;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d)&&!d.code_backing_ok);m->layout.code.write=false;
 for(unsigned i=1;i<=12;i++){h->fail_map=h->maps+i;assert(fmm_diagnose_code_write(&f,0xffc84010,0xffc84010,&d));h->fail_map=0;}
 assert(fmm_diagnose_code_write(0,0,0,&d)&&!d.code_backing_ok&&!d.walk.mode_ok);
 assert(!memcmp(before,h->bytes,(size_t)COUNT*4096));
 assert(fwm_release(m));free(before);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("CODE diagnostic: exact RX ownership, read/write walk, absent D/W, alias/leaf rejection, immutable bytes PASS");
}
