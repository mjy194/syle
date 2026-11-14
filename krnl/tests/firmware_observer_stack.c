#define _POSIX_C_SOURCE 200112L
#include "../firmware_observer_stack.h"
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
 unsigned char code[65536]={0},vars[4096]={0};struct sg_memory_ops ops={h,alloc,release,map,wb};assert(fwm_prepare(m,&ops,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x668,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0x4023);
 put(m,0x4000+8,0x8063);put(m,0x4000+16,0x9063);
 for(unsigned i=0;i<5;i++){put(m,0x8100+i*0x20,i==4?0:0x1100+(i+1)*0x20);put(m,0x8108+i*0x20,0x1000);}
 struct fos_result r;assert(fos_observe(&f,0x1100,0x1108,true,&r)&&r.count==4&&r.stop==FOS_LIMIT&&r.top_readable&&r.top_return==0x1000&&r.top_executable);
 assert(r.frames[0].frame==0x1100&&r.frames[0].previous==0x1120&&r.frames[0].return_executable);
 put(m,0x8140,0);assert(fos_observe(&f,0x1100,0,false,&r)&&r.count==3&&r.stop==FOS_END);
 put(m,0x8120,0x1100);assert(fos_observe(&f,0x1100,0,false,&r)&&r.count==2&&r.stop==FOS_CHAIN);
 assert(fos_observe(&f,UINT64_MAX-7,0,false,&r)&&!r.count&&r.stop==FOS_BAD_ADDRESS);
 assert(fos_observe(&f,0x1101,0,false,&r)&&r.stop==FOS_BAD_ADDRESS);
 uint64_t value=99;assert(!fos_read_word(&f,0x800000000000,&value)&&value==99);
 /* Cross-page words require every byte and physical continuity. */
 uint8_t bytes[8]={1,2,3,4,5,6,7,8};assert(fwm_write_ram(m,0x8ffc,bytes,8));
 assert(fos_read_word(&f,0x1ffc,&value)&&value==UINT64_C(0x0807060504030201));
 put(m,0x4010,0xa063);value=99;assert(!fos_read_word(&f,0x1ffc,&value)&&value==99);
 put(m,0x4010,0x9063);put(m,0x4008,0x8043);assert(!fos_read_word(&f,0x1100,&value));
 put(m,0x4008,0xfee00063);assert(!fos_read_word(&f,0x1100,&value));
 put(m,0x4008,0xfffff063);assert(!fos_read_word(&f,0x1100,&value));
 put(m,0x4008,0x8063);
 /* Owned-RAM API rejects invalid ownership metadata, never exposes pointer. */
 uint64_t token=m->ram[8];m->ram[8]=0;assert(!fos_read_word(&f,0x1100,&value));m->ram[8]=token;
 uint64_t other=m->ram[9];m->ram[9]=token;assert(!fos_read_word(&f,0x1100,&value));m->ram[9]=other;
 assert(fos_observe(&f,0,0xfee00000,true,&r)&&!r.count&&!r.top_readable);
 assert(!fos_observe(0,0,0,false,&r)&&!fos_observe(&f,0,0,false,0));
 assert(fwm_release(m));free(h->bytes);free(h);free(m);puts("firmware_observer_stack: PASS (read-only bounded diagnostic)");
}
