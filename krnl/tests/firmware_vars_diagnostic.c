#define _POSIX_C_SOURCE 200112L
#include "../firmware_vars_rom.h"
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
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);vars[16]=0x8d;struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x1000,0x2023);put(m,0x2018,0x4023);put(m,0x4000+510*8,0x6023);put(m,0x6000,0xffc00063);
 uint64_t expected[]={0x2023,0x4023,0x6023,0xffc00063};uint64_t table[]={0x1000,0x2000,0x4000,0x6000};
 struct fmm_vars_diagnostic d;assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d));
 assert(d.backing_stop==FMM_VARS_VALID&&d.ept_count==4&&d.ept_permissions[3]==0x31&&d.mode_ok&&d.canonical&&d.read_ok&&d.write_ok&&d.vars_backing_ok&&d.count==4&&d.stop==FMM_DIAG_LEAF&&d.read_gpa==0xffc00010&&d.write_gpa==0xffc00010&&d.read_attributes==0x400);
 for(unsigned i=0;i<4;i++)assert(d.entries[i].value==expected[i]&&d.entries[i].table_gpa==table[i]);
 /* D0 hypothesis: the diagnostic exposes raw leaf, but original WRITE
  * validator still fails. It does not promote the read result to admission. */
 put(m,0x6000,0xffc00023);assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&d.read_ok&&!d.write_ok&&d.vars_backing_ok&&d.count==4&&d.entries[3].value==0xffc00023);
 assert(!fmm_validate_vars_write(&f,0xffc00010,1,48,0x58a,0xffc00010,0xffc00010,0));uint64_t unchanged=0;assert(fwm_read_ram(m,0x6000,&unchanged,8)&&unchanged==0xffc00023);
 /* PTE protection, translation mismatch and absent backing are separate. */
 put(m,0x6000,0xffc01061);assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&d.read_ok&&!d.write_ok&&d.read_gpa==0xffc01010&&d.vars_backing_ok);
 put(m,0x6000,0xffc00063);assert(fmm_diagnose_vars_write(&f,0xffc00010,0xfee00000,&d)&&d.write_ok&&!d.vars_backing_ok);
 uint64_t*leaf=(uint64_t*)(h->bytes+(m->tables[68]-BASE));uint64_t savedleaf=leaf[0];leaf[0]|=2;assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&d.write_ok&&!d.vars_backing_ok&&d.backing_stop==FMM_VARS_LEAF&&!d.ept_matches[3]);leaf[0]=savedleaf;
 put(m,0x1000,0x2003);assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&!d.read_ok&&!d.write_ok&&d.count==1&&d.stop==FMM_DIAG_ENTRY_SHAPE);put(m,0x1000,0x2023);
 put(m,0x1000,0xfee00023);assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&d.count==1&&d.stop==FMM_DIAG_TABLE_RANGE);put(m,0x1000,0x2023);
 uint64_t saved=m->ram[1];m->ram[1]=0xdead000;assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&!d.count&&d.stop==FMM_DIAG_TABLE_OWNER);m->ram[1]=saved;
 /* Two successful translation walks and8EPT reads precede trace reads. */
 h->fail_map=h->maps+17;assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&!d.count&&d.stop==FMM_DIAG_TABLE_READ);h->fail_map=0;
 assert(fmm_diagnose_vars_write(&f,0x800000000000ULL,0xffc00010,&d)&&d.mode_ok&&!d.canonical&&!d.count);
 f.cr4|=0x1000;assert(fmm_diagnose_vars_write(&f,0xffc00010,0xffc00010,&d)&&!d.mode_ok&&!d.count);f.cr4=0x2660;
 assert(!fmm_diagnose_vars_write(&f,0,0,0));assert(fmm_diagnose_vars_write(0,0,0,&d)&&!d.mode_ok);
 assert(fwm_release(m)&&!h->live);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("VARS diagnostics: owned read-only walk, D0 observed not admitted, separated backing/translation PASS");return 0;
}
