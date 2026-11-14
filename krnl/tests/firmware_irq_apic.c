#define _POSIX_C_SOURCE 200112L
#include "../firmware_irq_apic.h"
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

struct writes {unsigned calls,fail;uint32_t fields[3];uint64_t values[3];};
static bool wr(void*x,uint32_t field,uint64_t value){struct writes*w=x;assert(w->calls<3);w->fields[w->calls]=field;w->values[w->calls++]=value;return w->calls!=w->fail;}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536]={0},vars[4096]={0};
 struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={m,0x80000033,0x1000,0x2660,0x500,0,0xa09b,8,false};
 put(m,0x1000,0x2023);put(m,0x2018,0x4023);put(m,0x4000+511*8,0x5023);
 put(m,0x5000+511*8,0xfffff063);put(m,0x4000+503*8,0x6023);put(m,0x6000,0xfee00063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;
 struct fag_state d={0};assert(fag_init(&d));d.apic.irq.svr=0x10f;
 struct ftis_session s={0};assert(ftis_init(&s,1,4096,0,10000000));
 assert(ftis_sample(&s,1,4096,1000000));
 struct lei_snapshot snap={.rflags=0x202};uint64_t attempt;struct writes w={0};
 assert(ftis_before(&s,1,4096,&snap,&w,wr,&attempt)==FTIS_STAGED);w=(struct writes){0};
 assert(ftis_after(&s,1,4096,attempt,false,true,10,0,&w,wr)==FTIS_ACCOUNTED);
 assert(ftis_sample(&s,1,4096,2000000)&&s.owner.pending&&s.owner.in_service);
 uint64_t g[15]={0};struct fat_exit e={.rip=0xfffffff0,.qualification=0x181,
 .linear=0xfee00110,.physical=0xfee00110,.reason=48,.dr7=0x400,.rflags=0x10293};
 rom[0]=0x8b;rom[1]=0x03;g[1]=e.linear;g[0]=UINT64_MAX;w=(struct writes){0};
 assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_COMMITTED&&g[0]==1);
 assert(d.apic.irq.isr[1]==0&&d.apic.irq.irr[1]==0);
 g[1]=e.linear=e.physical=0xfee00210;w=(struct writes){0};
 assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_COMMITTED&&g[0]==1);
 bool ready;assert(fiap_select(&d,&s,&ready)&&!ready);
 rom[0]=0x89;rom[1]=0x33;g[1]=e.linear=e.physical=0xfee000b0;g[4]=0;e.qualification=0x182;
 for(unsigned fail=1;fail<=2;fail++){
  struct ftis_session copy=s;struct fag_state config=d;w=(struct writes){.fail=fail};
  assert(fiap_mmio(&config,&copy,g,&f,&e,&w,wr)==FAT_FATAL&&copy.failed);
  assert(copy.owner.pending&&copy.owner.in_service&&copy.owner.terminal);
 }
 w=(struct writes){0};assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_COMMITTED);
 assert(!s.owner.in_service&&s.owner.pending&&fiap_select(&d,&s,&ready)&&ready);
 g[1]=e.linear=e.physical=0xfee00080;g[4]=0x20;w=(struct writes){0};
 assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_COMMITTED&&d.apic.irq.tpr==0x20);
 assert(fiap_select(&d,&s,&ready)&&!ready&&s.owner.pending);
 g[1]=e.linear=e.physical=0xfee00380;g[4]=1;w=(struct writes){0};
 assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_REJECTED&&!w.calls);
 /* Empty EOI, current-count reads and forbidden writes never clear pending. */
 g[1]=e.linear=e.physical=0xfee000b0;g[4]=0;w=(struct writes){0};
 struct fti_owner owner_before=s.owner;
 assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_COMMITTED&&!memcmp(&owner_before,&s.owner,sizeof owner_before));
 rom[0]=0x8b;rom[1]=0x03;g[1]=e.linear=e.physical=0xfee00390;e.qualification=0x181;
 w=(struct writes){0};assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_COMMITTED&&g[0]==10000000);
 assert(!memcmp(&owner_before,&s.owner,sizeof owner_before));
 rom[0]=0x89;rom[1]=0x33;e.qualification=0x182;g[1]=e.linear=e.physical=0xfee00080;g[4]=0;
 for(unsigned fail=1;fail<=2;fail++){
  struct fag_state copy=d;struct ftis_session session=s;w=(struct writes){.fail=fail};
  assert(fiap_mmio(&copy,&session,g,&f,&e,&w,wr)==FAT_FATAL&&copy.apic.irq.tpr==d.apic.irq.tpr);
 }
 const unsigned forbidden[]={0xf0,0x320,0x380,0x3e0,0x300,0x310,0x3f0};
 for(unsigned i=0;i<sizeof forbidden/sizeof forbidden[0];i++){
  g[1]=e.linear=e.physical=0xfee00000+forbidden[i];w=(struct writes){0};
  assert(fiap_mmio(&d,&s,g,&f,&e,&w,wr)==FAT_REJECTED&&!w.calls&&!memcmp(&owner_before,&s.owner,sizeof owner_before));
 }
 assert(fwm_release(m)&&!h->live);free(h->bytes);free(h);free(m);
 puts("IRQ APIC overlay: sole owner projection, EOI preserves pending, TPR gate, failure no retirement PASS");
}
