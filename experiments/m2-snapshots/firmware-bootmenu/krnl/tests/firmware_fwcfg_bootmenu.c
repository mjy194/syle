#define _POSIX_C_SOURCE 200112L
#include "firmware_fwcfg_transaction.h"
#include "guest_mmio_mov.h"
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

struct commit {bool fail_ram,fail_vm;unsigned ram,vm;uint64_t rip;};
static bool ramwr(void*p,uint8_t*t,const uint8_t*b,unsigned n){struct commit*c=p;c->ram++;for(unsigned i=0;i<n;i++){t[i]=b[i];if(c->fail_ram)return false;}return true;}
static bool vmwr(void*p,uint32_t f,uint64_t v){struct commit*c=p;c->vm++;assert(f==0x681e);c->rip=v;return !c->fail_vm;}
int main(void){struct ffc_state scalar={0};assert(ffc_init(&scalar,0x8000000,1));uint64_t r=UINT64_C(0xabcdef123400000e),saved=r;struct ffc_state old=scalar;
#ifdef FFC_BOOT_MENU_DEFAULT
 (void)old;assert(ffc_io(&scalar,0x510,2,false,&r)&&r==saved&&scalar.selector==14&&!scalar.cursor);
 for(unsigned i=0;i<5;i++){r=UINT64_MAX;assert(ffc_io(&scalar,0x511,1,true,&r)&&r==UINT64_C(0xffffffffffffff00)&&scalar.cursor==i+1);}
 r=14;assert(ffc_io(&scalar,0x510,2,false,&r)&&!scalar.cursor);scalar.cursor=UINT64_MAX;old=scalar;assert(!ffc_io(&scalar,0x511,1,true,&r)&&!memcmp(&scalar,&old,sizeof old));
#else
 assert(!ffc_io(&scalar,0x510,2,false,&r)&&r==saved&&!memcmp(&scalar,&old,sizeof old));
#endif
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536]={0},vars[4096]={0};struct sg_memory_ops ops={h,alloc,release,map,wb};assert(fwm_prepare(m,&ops,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0x4023);put(m,0x4000,0x8063);put(m,0x4000+16*8,0x10063);
 struct fwc_state initial={0};assert(ffc_init(&initial.stream,m->layout.ram.size,1));
 struct fwc_exit e={.rip=0x10000,.qualification=0x5100001,.length=2,.flags=2,.dr7=0x400,.es_ar=0xc093,.linear=0x40,.info=0x100,.info_supported=true};
 uint8_t sel[]={0x66,0xef};assert(fwm_write_ram(m,0x10000,sel,2));uint64_t g[15]={UINT64_C(0xabcdef000000000e)};g[3]=0x510;uint64_t original[15];memcpy(original,g,sizeof g);struct fwc_state s=initial;struct commit c={0};
#ifdef FFC_BOOT_MENU_DEFAULT
 assert(fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&s.stream.selector==14&&!memcmp(g,original,sizeof g));struct fwc_state selected=s;
 s=initial;c=(struct commit){.fail_vm=true};assert(!fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&s.failed&&!s.stream.selector&&!memcmp(g,original,sizeof g));
 uint8_t rep[]={0xf3,0x6c};assert(fwm_write_ram(m,0x10000,rep,2));e.qualification=0x5110038;
 g[0]=UINT64_C(0xabcdef1234567890);g[2]=2;g[3]=0x511;g[5]=0x40;memcpy(original,g,sizeof g);
 for(unsigned fault=0;fault<3;fault++){uint8_t sentinel[2]={0xaa,0xbb};assert(fwm_write_ram(m,0x8040,sentinel,2));s=selected;memcpy(g,original,sizeof g);c=(struct commit){.fail_ram=fault==1,.fail_vm=fault==2};
  bool ok=fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr);uint8_t data[2];assert(fwm_read_ram(m,0x8040,data,2));
  if(!fault){assert(ok&&s.stream.cursor==2&&g[2]==0&&g[5]==0x42&&g[0]==original[0]&&!data[0]&&!data[1]);for(unsigned i=0;i<15;i++)if(i!=2&&i!=5)assert(g[i]==original[i]);}
  else {assert(!ok&&s.failed&&!s.stream.cursor&&!memcmp(g,original,sizeof g)&&data[0]==0);unsigned calls=c.vm;assert(!fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&c.vm==calls);}
 }
#else
 assert(!fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&s.failed&&!c.vm&&!c.ram&&!memcmp(g,original,sizeof g));
#endif
 uint16_t deny[]={2,4,6,7,9,10,12,13,16,17,18,19,21,22,24,0x400e,0x800e,0xffff};
 scalar=(struct ffc_state){0};assert(ffc_init(&scalar,0x8000000,1));for(unsigned i=0;i<sizeof deny/sizeof *deny;i++){r=deny[i];assert(!ffc_io(&scalar,0x510,2,false,&r));}
 r=0;assert(!ffc_io(&scalar,0x514,8,false,&r)&&!ffc_io(&scalar,0x511,1,false,&r));
 assert(fwm_release(m));free(h->bytes);free(h);free(m);puts("fwcfg boot menu default: PASS scalar/selector/REP2/partial failures");}
