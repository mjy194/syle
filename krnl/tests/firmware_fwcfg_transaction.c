#define _POSIX_C_SOURCE 200112L
#include "../firmware_fwcfg_transaction.h"
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

struct commit {bool fail_ram,fail_vm;unsigned ram,vm;uint64_t rip;};
static bool ramwr(void*p,uint8_t*t,const uint8_t*b,unsigned n){struct commit*c=p;c->ram++;for(unsigned i=0;i<n;i++){t[i]=b[i];if(c->fail_ram)return false;}return true;}
static bool vmwr(void*p,uint32_t f,uint64_t v){struct commit*c=p;c->vm++;assert(f==0x681e);c->rip=v;return !c->fail_vm;}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536]={0},vars[4096]={0};struct sg_memory_ops o={h,alloc,release,map,wb};
 assert(fwm_prepare(m,&o,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0x4023);put(m,0x4000,0x8063);put(m,0x4000+16*8,0x10063);
 uint8_t ins[]={0xf3,0x6c};assert(fwm_write_ram(m,0x10000,ins,2));
 struct fwc_state initial={0};assert(ffc_init(&initial.stream,m->layout.ram.size,1));
 struct fwc_exit e={.rip=0x10000,.qualification=0x5110038,.length=2,.flags=2,.dr7=0x400,.es_ar=0xc093,.linear=0x40,.info=0x100,.info_supported=true};
 uint64_t baseg[15]={0};baseg[0]=0x12345678;baseg[2]=4;baseg[3]=0x511;baseg[5]=0x40;
 uint64_t g[15];memcpy(g,baseg,sizeof g);struct fwc_state s=initial;struct commit c={0};
 assert(fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr));assert(s.stream.cursor==4&&g[2]==0&&g[5]==0x44&&g[0]==baseg[0]&&c.ram==1&&c.vm==1&&c.rip==0x10002);
 uint8_t result[4];assert(fwm_read_ram(m,0x8040,result,4)&&!memcmp(result,"QEMU",4));
 for(unsigned bad=0;bad<2;bad++){
  uint8_t zero[4]={0};assert(fwm_write_ram(m,0x8040,zero,4));s=initial;memcpy(g,baseg,sizeof g);c=(struct commit){.fail_ram=bad==0,.fail_vm=bad==1};
  assert(!fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&s.failed&&s.stream.cursor==0&&!memcmp(g,baseg,sizeof g));
  assert(fwm_read_ram(m,0x8040,result,4)&&result[0]=='Q'); /* No rollback claim. */
  assert(!fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr));
 }
 for(unsigned bad=0;bad<8;bad++){
  s=initial;memcpy(g,baseg,sizeof g);c=(struct commit){0};struct fwc_exit x=e;
  if(bad==0)x.info_supported=false;
  if(bad==1)x.info=0x80;
  if(bad==2)x.es_ar|=0x10000;
  if(bad==3)x.flags|=0x400;
  if(bad==4)x.flags|=0x10000;
  if(bad==5)x.linear++;
  if(bad==6)g[2]=65;
  if(bad==7)x.es_base=1;
  assert(!fwc_apply(&s,g,&f,&x,true,&c,ramwr,vmwr)&&s.failed&&!c.ram&&!c.vm&&s.stream.cursor==0);
 }
 s=initial;memcpy(g,baseg,sizeof g);g[2]=0;c=(struct commit){0};assert(fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&!c.ram&&c.vm==1&&s.stream.cursor==0&&g[5]==0x40);
 s=initial;memcpy(g,baseg,sizeof g);c=(struct commit){0};assert(!fwc_apply(&s,g,&f,&e,false,&c,ramwr,vmwr)&&!memcmp(&s,&initial,sizeof s)&&!c.ram&&!c.vm);
 /* Selector then scalar data use the same immutable stream model. */
 uint8_t sel[]={0x66,0xef};assert(fwm_write_ram(m,0x10000,sel,2));s=initial;memcpy(g,baseg,sizeof g);g[0]=1;g[3]=0x510;e.qualification=0x5100001;c=(struct commit){0};
 assert(fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&s.stream.selector==1&&s.stream.cursor==0);
 uint8_t in=0xec;assert(fwm_write_ram(m,0x10000,&in,1));e.length=1;e.qualification=0x5110008;g[3]=0x511;g[0]=0x123400;c=(struct commit){0};
 assert(fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr)&&g[0]==0x123401&&s.stream.cursor==1);
 assert(fwm_release(m)&&!h->live);free(h->bytes);free(h);free(m);
 puts("firmware fwcfg transaction: REP/scalar, owned RAM, fatal partial commits PASS");
}
