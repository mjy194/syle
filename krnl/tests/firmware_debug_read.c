#define _POSIX_C_SOURCE 200112L
#include "../firmware_debug_read.h"
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





struct callback {unsigned calls,fail;bool reenter;struct fdr_bank*b;struct fdr_gate*gate;uint64_t*g;struct fmm_memory*m;struct fat_exit*e;};
static bool wr(void*x,uint32_t field,uint64_t value){struct callback*c=x;++c->calls;
 assert((field==0x6820&&value==0x46)||(field==0x681e&&value==0xfffffff3));
 if(c->reenter)assert(fdr_read(c->b,c->gate,c->g,c->m,c->e,3,true,c,wr)==FAT_FATAL);
 return c->calls!=c->fail;
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x668,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0x0f;rom[1]=0x21;rom[2]=0xc0;
 struct fdr_bank b={0};assert(fdr_init(&b,7,0x1234));struct fdr_bank pristine=b;
 struct fdr_gate gate={.domain=7,.vmcs=0x1234,.primary_controls=1u<<23,.debug_events_absent=true};
 struct fat_exit e={.rip=0xfffffff0,.qualification=0x10,.rflags=0x10046,.dr7=0x400,.reason=29};
 uint64_t g[15];for(unsigned i=0;i<15;i++)g[i]=UINT64_MAX-i;
 struct callback c={.b=&b,.gate=&gate,.g=g,.m=&f,.e=&e};
 for(unsigned dr=0;dr<8;dr++){if(dr==4||dr==5)continue;rom[2]=0xc0|(dr<<3);e.qualification=0x10|dr;c.calls=0;g[0]=UINT64_MAX;
  assert(fdr_read(&b,&gate,g,&f,&e,3,true,&c,wr)==FAT_COMMITTED&&c.calls==2);
  assert(g[0]==(dr==6?UINT64_C(0xffff0ff0):dr==7?0x400:0)&&!memcmp(&b,&pristine,sizeof b));
  for(unsigned i=1;i<15;i++)assert(g[i]==UINT64_MAX-i);
 }
 rom[2]=0xc0;e.qualification=0x10;
 for(unsigned k=0;k<20;k++){struct fdr_gate bg=gate;struct fmm_memory fm=f;struct fat_exit be=e;unsigned len=3;
  switch(k){case 0:be.qualification=0;break;case 1:be.qualification=0x14;rom[2]=0xe0;break;
  case 2:be.qualification=0x15;rom[2]=0xe8;break;case 3:be.qualification=0x110;break;
  case 4:rom[0]=0x48;break;case 5:rom[1]=0x23;break;case 6:rom[2]=0xc1;break;
  case 7:bg.domain++;break;case 8:bg.vmcs++;break;case 9:bg.debugctl=1;break;
  case 10:bg.primary_controls=0;break;case 11:bg.debug_events_absent=false;break;
  case 12:be.dr7=0x401;break;case 13:be.pending_debug=1;break;case 14:be.rflags|=0x100;break;
  case 15:be.blocking=1;break;case 16:be.entry_event=0x80000020;break;case 17:len=4;break;
  case 18:fm.cs_selector=3;break;case 19:be.vectoring=0x80000020;break;}
  uint64_t old=g[0];c.calls=0;assert(fdr_read(&b,&bg,g,&fm,&be,len,true,&c,wr)==FAT_REJECTED&&!c.calls&&g[0]==old&&!memcmp(&b,&pristine,sizeof b));
  rom[0]=0x0f;rom[1]=0x21;rom[2]=0xc0;
 }
 for(unsigned k=1;k<=3;k++){b=pristine;g[0]=UINT64_MAX;c.calls=0;c.fail=k<=2?k:0;c.reenter=k==3;
  assert(fdr_read(&b,&gate,g,&f,&e,3,true,&c,wr)==FAT_FATAL&&b.failed&&g[0]==UINT64_MAX&&c.calls==(k==2?2:1));
  assert(!fdr_init(&b,7,0x1234));unsigned n=c.calls;assert(fdr_read(&b,&gate,g,&f,&e,3,true,&c,wr)==FAT_REJECTED&&c.calls==n);
 }
 b=pristine;c.fail=0;c.reenter=false;c.calls=0;e.rflags=0x46;
 assert(fdr_read(&b,&gate,g,&f,&e,3,true,&c,wr)==FAT_COMMITTED&&c.calls==1);
 assert(!fdr_init(&b,7,0x1234));assert(!fdr_init(0,7,0x1234));
 assert(fwm_release(m));free(h->bytes);free(h);free(m);free(code);free(vars);
 puts("firmware_debug_read: PASS (virtual clean bank only; no host debug state)");
}
