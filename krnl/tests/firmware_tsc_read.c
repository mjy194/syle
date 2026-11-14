#define _POSIX_C_SOURCE 200112L
#include "../firmware_tsc_read.h"
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





struct cb {unsigned calls,fail;bool recursive;struct ftsc_state*s;struct ftsc_sample*t;uint64_t*g;struct fmm_memory*m;struct fat_exit*e;};
static bool wr(void*x,uint32_t field,uint64_t value){struct cb*c=x;++c->calls;assert((field==0x6820&&value==0x46)||(field==0x681e&&value==0xfffffff2));
 if(c->recursive)assert(ftsc_read(c->s,c->t,c->g,c->m,c->e,2,true,c,wr)==FAT_FATAL);
 return c->calls!=c->fail;}
int main(void){struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x668,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0x0f;rom[1]=0x31;
 struct ftsc_state s={0};assert(ftsc_init(&s,7,8,100,UINT64_MAX,0));struct ftsc_state initial=s;
 struct ftsc_sample t={7,8,100,0,true};struct fat_exit e={.rip=0xfffffff0,.reason=16,.rflags=0x10046,.dr7=0x400};uint64_t g[15];for(unsigned i=0;i<15;i++)g[i]=UINT64_MAX;
 struct cb c={.s=&s,.t=&t,.g=g,.m=&f,.e=&e};uint64_t refs[]={100,101,UINT64_C(0x100000064),UINT64_MAX};
 for(unsigned k=0;k<4;k++){t.reference=refs[k];assert(ftsc_sample(&s,7,8,0,t.reference));c.calls=0;assert(ftsc_read(&s,&t,g,&f,&e,2,true,&c,wr)==FAT_COMMITTED&&s.last==refs[k]);
  uint64_t result=refs[k]-100;assert(g[0]==(uint32_t)result&&g[3]==(uint32_t)(result>>32));for(unsigned i=0;i<15;i++)if(i!=0&&i!=3)assert(g[i]==UINT64_MAX);}
 for(unsigned k=0;k<5;k++){s=initial;t=(struct ftsc_sample){7,8,101,0,true};c.calls=0;
  switch(k){case 0:t.source_admitted=false;break;case 1:t.domain++;break;case 2:t.epoch++;break;case 3:t.reference=99;break;case 4:s.max_gap=1;t.reference=102;break;}
  assert(ftsc_read(&s,&t,g,&f,&e,2,true,&c,wr)==FAT_FATAL&&s.failed&&!c.calls&&!ftsc_init(&s,7,8,100,1,0));}
 for(unsigned k=0;k<10;k++){s=initial;t=(struct ftsc_sample){7,8,101,0,true};assert(ftsc_sample(&s,7,8,0,101));struct fat_exit be=e;struct fmm_memory fm=f;unsigned len=2;c.calls=0;
  switch(k){case 0:rom[0]=0x66;break;case 1:rom[1]=0x32;break;case 2:len=3;break;case 3:be.reason=51;break;case 4:be.rflags|=0x100;break;case 5:be.pending_debug=1;break;case 6:be.entry_event=0x80000020;break;case 7:fm.cs_selector=3;break;case 8:fm.cr4|=4;break;case 9:be.qualification=1;break;}
  assert(ftsc_read(&s,&t,g,&fm,&be,len,true,&c,wr)==FAT_REJECTED&&!c.calls&&s.last==101);rom[0]=0x0f;rom[1]=0x31;}
 for(unsigned k=1;k<=3;k++){s=initial;t=(struct ftsc_sample){7,8,123,0,true};assert(ftsc_sample(&s,7,8,0,123));g[0]=g[3]=UINT64_MAX;c.calls=0;c.fail=k<=2?k:0;c.recursive=k==3;
  assert(ftsc_read(&s,&t,g,&f,&e,2,true,&c,wr)==FAT_FATAL&&s.failed&&s.last==123&&g[0]==UINT64_MAX&&g[3]==UINT64_MAX&&c.calls==(k==2?2:1));}
 /* Many admitted samples with no RDTSC keep maxgap continuity. */
 s=initial;s.max_gap=10;
 for(uint64_t ref=110;ref<=1100;ref+=10)assert(ftsc_sample(&s,7,8,0,ref));
 t=(struct ftsc_sample){7,8,1100,0,true};c.fail=0;c.recursive=false;c.calls=0;
 assert(ftsc_read(&s,&t,g,&f,&e,2,true,&c,wr)==FAT_COMMITTED&&g[0]==1000);
 for(unsigned k=0;k<4;k++){s=initial;s.max_gap=10;
  assert(!ftsc_sample(&s,k==0?9:7,k==1?9:8,k==2?1:0,k==3?111:101)&&s.failed);
 }
 s=initial;assert(!ftsc_sample(&s,7,8,0,99)&&s.failed);
 assert(fwm_release(m));free(h->bytes);free(h);free(m);free(code);free(vars);puts("firmware_tsc_read: PASS (virtual10MHz, no host TSC)");}
