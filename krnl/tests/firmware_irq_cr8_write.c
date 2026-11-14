#define _POSIX_C_SOURCE 200112L
#include "../firmware_irq_cr8_write.h"
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





struct callback {unsigned calls,fail;struct ftis_session*s;bool reenter;struct fag_state*d;uint64_t*g;struct fmm_memory*m;struct fat_exit*e;};
static bool wr(void*x,uint32_t field,uint64_t value){struct callback*c=x;++c->calls;
 assert((field==0x6820&&value==0x46)||(field==0x681e&&value==0xfffffff4));
 if(c->reenter)assert(fir8_write(c->d,c->s,c->g,c->m,c->e,4,true,c,wr)==FAT_FATAL);
 return c->calls!=c->fail;
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
#ifdef SYLE_FMM_DE_CONFIG
 f.cr4|=8;
#endif
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;uint8_t insn[]={0x44,0x0f,0x22,0xc0};memcpy(rom,insn,4);
 struct fag_state d={0};assert(fag_init(&d));d.apic.irq.svr=0x10f;
 struct ftis_session s={0};assert(ftis_init(&s,7,0x1234,0,UINT64_MAX));s.owner.in_service=true;
 struct fat_exit e={.rip=0xfffffff0,.qualification=0x8,.rflags=0x10046,.dr7=0x400,.reason=28};
 uint64_t g[15];for(unsigned i=0;i<15;i++)g[i]=UINT64_MAX-i;
 struct callback c={.s=&s,.d=&d,.g=g,.m=&f,.e=&e};struct ftis_session pristine=s;
 for(unsigned state=0;state<3;state++)for(unsigned t=0;t<256;t++)for(unsigned v=0;v<16;v++){
  s=pristine;s.owner.pending=state!=0;s.owner.in_service=state!=1;
  struct fti_owner old=s.owner;d.apic.irq.tpr=t;g[0]=v;c.calls=0;
  assert(fir8_write(&d,&s,g,&f,&e,4,true,&c,wr)==FAT_COMMITTED);
  assert(g[0]==v&&d.apic.irq.tpr==(v<<4)&&c.calls==2&&!memcmp(&old,&s.owner,sizeof old));
  for(unsigned i=1;i<15;i++)assert(g[i]==UINT64_MAX-i);
 }
 /* Explicit priority regression: 2f -> CR8=1 gives TPR10, not 1f. */
 for(unsigned isr=0;isr<2;isr++){
  s=pristine;s.owner.pending=true;s.owner.in_service=isr!=0;
  struct fti_owner unchanged=s.owner;d.apic.irq.tpr=0x2f;g[0]=1;c.calls=0;
  bool ready=false;assert(fiap_select(&d,&s,&ready)&&!ready);
  assert(fir8_write(&d,&s,g,&f,&e,4,true,&c,wr)==FAT_COMMITTED&&d.apic.irq.tpr==0x10);
  assert(!memcmp(&unchanged,&s.owner,sizeof unchanged));
  assert(fiap_select(&d,&s,&ready)&&ready==(isr==0));
 }
 s=pristine;g[0]=0;d.apic.irq.tpr=0x5f;
 for(unsigned k=0;k<15;k++){struct fmm_memory bad=f;struct fat_exit be=e;unsigned len=4;
  switch(k){case 0:rom[0]=0x4c;break;case 1:rom[2]=0x20;break;case 2:rom[3]=0xc1;break;
  case 3:be.qualification=0x18;break;case 4:be.reason=31;break;case 5:len=3;break;
  case 6:bad.efer=0x100;break;case 7:bad.cs_selector=3;break;case 8:be.rflags|=0x100;break;
  case 9:be.blocking=1;break;case 10:be.entry_event=0x80000020;break;
  case 11:be.pending_debug=1;break;case 12:be.dr7=0x401;break;
  case 13:bad.cr4|=0x1000;break;case 14:be.vectoring=0x80000020;break;}
  c.calls=0;uint64_t before=g[0];assert(fir8_write(&d,&s,g,&bad,&be,len,true,&c,wr)==FAT_REJECTED&&!c.calls&&g[0]==before);
  assert(!memcmp(&s,&pristine,sizeof s));memcpy(rom,insn,4);
 }
 uint64_t badvals[]={16,0x100,UINT64_C(1)<<32,UINT64_C(1)<<63,UINT64_MAX};
 for(unsigned i=0;i<sizeof badvals/sizeof *badvals;i++){g[0]=badvals[i];c.calls=0;
  assert(fir8_write(&d,&s,g,&f,&e,4,true,&c,wr)==FAT_REJECTED&&!c.calls&&g[0]==badvals[i]&&d.apic.irq.tpr==0x5f);
 }
 for(unsigned k=1;k<=3;k++){s=pristine;g[0]=0;c.calls=0;c.fail=k<=2?k:0;c.reenter=k==3;
  assert(fir8_write(&d,&s,g,&f,&e,4,true,&c,wr)==FAT_FATAL&&s.failed&&s.owner.terminal&&s.owner.in_service&&!s.owner.pending&&!s.owner.reserved&&g[0]==0&&d.apic.irq.tpr==0x5f);
  assert(c.calls==(k==2?2:1));unsigned n=c.calls;assert(fir8_write(&d,&s,g,&f,&e,4,true,&c,wr)==FAT_REJECTED&&c.calls==n);
 }
 s=pristine;c.fail=0;c.reenter=false;c.calls=0;e.rflags=0x46;
 assert(fir8_write(&d,&s,g,&f,&e,4,true,&c,wr)==FAT_COMMITTED&&c.calls==1);
 assert(fwm_release(m));free(h->bytes);free(h);free(m);free(code);free(vars);
 puts("firmware_irq_cr8_write: PASS (host-only; observed opcode still requires native diagnostic)");
}
