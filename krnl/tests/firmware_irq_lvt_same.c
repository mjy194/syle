#define _POSIX_C_SOURCE 200112L
#include "../firmware_irq_lvt_same.h"
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





struct callback {unsigned calls,fail;bool reenter;struct fag_state*d;struct ftis_session*s;uint64_t*g;struct fmm_memory*m;struct fat_exit*e;};
static bool wr(void*x,uint32_t field,uint64_t value){struct callback*c=x;++c->calls;
 assert((field==0x6820&&value==0x46)||(field==0x681e&&value==0xfffffff2));
 if(c->reenter)assert(fils_write(c->d,c->s,c->g,c->m,c->e,true,c,wr)==FAT_FATAL);
 return c->calls!=c->fail;
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x668,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06fb8,0xfee000e3);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0x89;rom[1]=0x01;
 struct fag_state d={0};assert(fag_init(&d));d.apic.irq.svr=0x10f;
 struct ftis_session s={0};assert(ftis_init(&s,7,0x1234,0,UINT64_MAX));struct ftis_session pristine=s;
 struct fat_exit e={.rip=0xfffffff0,.qualification=0x582,.linear=0xfee00320,.physical=0xfee00320,.rflags=0x10046,.dr7=0x400,.reason=48};
 uint64_t g[15]={0x20020};g[2]=0xfee00320;uint64_t saved[15];memcpy(saved,g,sizeof g);
 struct callback c={.d=&d,.s=&s,.g=g,.m=&f,.e=&e};
 for(unsigned state=0;state<4;state++){s=pristine;s.owner.pending=state&1;s.owner.in_service=!!(state&2);
  struct ftis_session before=s;struct fag_state device=d;c.calls=0;
  assert(fils_write(&d,&s,g,&f,&e,true,&c,wr)==FAT_COMMITTED&&c.calls==2);
  assert(!memcmp(&before,&s,sizeof s)&&!memcmp(&device,&d,sizeof d)&&!memcmp(saved,g,sizeof g));
 }
 /* Sample before and after multiple due boundaries; never restores old phase. */
 s=pristine;assert(ftis_sample(&s,7,0x1234,999999));struct ftis_session before=s;c.calls=0;
 assert(fils_write(&d,&s,g,&f,&e,true,&c,wr)==FAT_COMMITTED&&!memcmp(&s,&before,sizeof s));
 assert(ftis_sample(&s,7,0x1234,3500000)&&s.owner.pending&&s.owner.expirations==3);before=s;c.calls=0;
 assert(fils_write(&d,&s,g,&f,&e,true,&c,wr)==FAT_COMMITTED&&!memcmp(&s,&before,sizeof s));
 for(unsigned k=0;k<16;k++){s=pristine;struct fmm_memory bad=f;struct fat_exit be=e;c.calls=0;
  switch(k){case 0:g[0]=0x30020;break;case 1:g[0]=0;break;case 2:rom[0]=0x88;break;
  case 3:rom[0]=0x8b;break;case 4:be.physical++;break;case 5:be.linear++;break;
  case 6:be.qualification=0x581;break;case 7:bad.cs_selector=3;break;
  case 8:bad.cr4|=0x1000;break;case 9:be.rflags|=0x100;break;
  case 10:be.blocking=1;break;case 11:put(m,0x7c06fb8,0xfee000a3);break;
  case 12:put(m,0x7c02018,0x7c06003);break;
  case 13:be.entry_event=0x80000020;break;case 14:s.owner.reserved=true;break;case 15:put(m,0x7c06fb8,0x2000e3);break;}
  assert(fils_write(&d,&s,g,&bad,&be,true,&c,wr)==FAT_REJECTED&&!c.calls);
  g[0]=0x20020;rom[0]=0x89;put(m,0x7c06fb8,0xfee000e3);put(m,0x7c02018,0x7c06023);
 }
 for(unsigned k=1;k<=3;k++){s=pristine;s.owner.pending=true;s.owner.in_service=true;c.calls=0;c.fail=k<=2?k:0;c.reenter=k==3;
  assert(fils_write(&d,&s,g,&f,&e,true,&c,wr)==FAT_FATAL&&s.failed&&s.owner.terminal&&s.owner.pending&&s.owner.in_service&&c.calls==(k==2?2:1));
  assert(!memcmp(g,saved,sizeof g));
 }
 assert(fwm_release(m));free(h->bytes);free(h);free(m);free(code);free(vars);
 puts("firmware_irq_lvt_same: PASS (runtime matching only, no timer reconfiguration)");
}
