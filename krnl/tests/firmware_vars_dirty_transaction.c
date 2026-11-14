#define _POSIX_C_SOURCE 200112L
#include "../firmware_vars_dirty_transaction.h"
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




struct integration {struct firmware_memory*m;struct fvr_state*vars;unsigned stage,samples,cas,writes;unsigned fail_sample,fail_write;bool uncertain,cas_conflict,ancestor_change;char order[12];unsigned events;};
static void logevent(struct integration*c,char v){assert(c->vars->stage==c->stage&&!c->vars->failed);c->order[c->events++]=v;}
static bool isample(void*x){struct integration*c=x;logevent(c,'S');c->samples++;if(c->ancestor_change&&c->samples==1)put(c->m,0x7c01000,0x7c02033);return c->samples!=c->fail_sample;}
static enum fwd_cas_result icas(void*x,uint64_t*p,uint64_t old,uint64_t desired){struct integration*c=x;logevent(c,'D');c->cas++;if(c->uncertain){*p=desired;return FWD_CAS_UNCERTAIN;}if(c->cas_conflict)*p=old^0x10;return fwd_atomic_cas(p,old,desired);}
static bool iwrite(void*x,uint32_t field,uint64_t value){struct integration*c=x;logevent(c,field==0x6820?'F':'R');c->writes++;assert((field==0x6820&&value==0x46)||(field==0x681e&&value==0xfffffff3));return c->writes!=c->fail_write;}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);vars[16]=0x8d;struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06ff0,0xffc000a3);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0xc6;rom[1]=0x03;rom[2]=0x50;
 struct fvr_state probe={0};assert(fvr_init(&probe,m));uint64_t g[15]={0};g[1]=probe.address;
 struct fat_exit e={.rip=0xfffffff0,.qualification=0x58a,.linear=probe.address,.physical=probe.address,.rflags=0x10046,.dr7=0x400,.reason=48};

 struct fvr_state pristine=probe;struct fvdt_state*state=calloc(1,sizeof*state);assert(state);struct integration c={.m=m,.vars=&probe};
 uint64_t before[15];memcpy(before,g,sizeof g);
 assert(fvdt_apply(state,&probe,g,&f,&e,true,true,&c,isample,icas,iwrite)==FAT_COMMITTED&&probe.stage==1&&state->transaction.complete&&!probe.failed&&!memcmp(g,before,sizeof g)&&!memcmp(c.order,"SDSFR",5));
 unsigned events=c.events;assert(fvdt_apply(state,&probe,g,&f,&e,true,true,&c,isample,icas,iwrite)==FAT_REJECTED&&c.events==events&&probe.stage==1);
 /* Independent fresh state for second command. Never reuse a consumed state. */
 memset(state,0,sizeof*state);rom[2]=0x70;put(m,0x7c06ff0,0xffc000a3);c=(struct integration){.m=m,.vars=&probe,.stage=1};
 assert(fvdt_apply(state,&probe,g,&f,&e,true,true,&c,isample,icas,iwrite)==FAT_COMMITTED&&probe.stage==2);
 rom[2]=0x50;
 for(unsigned failure=0;failure<7;failure++){
  memset(state,0,sizeof*state);probe=pristine;put(m,0x7c06ff0,0xffc000a3);put(m,0x7c01000,0x7c02023);c=(struct integration){.m=m,.vars=&probe};
  if(failure==0)c.fail_sample=1;
  if(failure==1)c.fail_sample=2;
  if(failure==2)c.uncertain=true;
  if(failure==3)c.cas_conflict=true;
  if(failure==4)c.fail_write=1;
  if(failure==5)c.fail_write=2;
  if(failure==6)c.ancestor_change=true;
  assert(fvdt_apply(state,&probe,g,&f,&e,true,true,&c,isample,icas,iwrite)==FAT_FATAL&&probe.failed&&!probe.stage&&state->transaction.failed&&!state->transaction.complete&&!memcmp(g,before,sizeof g));
  uint64_t actual;assert(fwm_read_ram(m,0x7c06ff0,&actual,8));
  assert(actual==(failure==0||failure==6?0xffc000a3:failure==3?0xffc000b3:0xffc000e3));
  if(failure==2)assert(!state->transaction.dirty_changed&&actual==0xffc000e3);
  if(failure==6)assert(!c.cas&&!c.writes);
  events=c.events;assert(fvdt_apply(state,&probe,g,&f,&e,true,true,&c,isample,icas,iwrite)==FAT_FATAL&&c.events==events);
 }
 /* Capture/no-D/admission rejection has no irreversible effects. */
 put(m,0x7c01000,0x7c02023);put(m,0x7c06ff0,0xffc000e3);probe=pristine;memset(state,0,sizeof*state);c=(struct integration){.m=m,.vars=&probe};
 assert(fvdt_apply(state,&probe,g,&f,&e,true,true,&c,isample,icas,iwrite)==FAT_REJECTED&&!state->transaction.consumed&&!probe.failed&&!c.events);
 put(m,0x7c06ff0,0xffc000a3);e.qualification=0x582;assert(fvdt_apply(state,&probe,g,&f,&e,true,true,&c,isample,icas,iwrite)==FAT_REJECTED&&!c.events);e.qualification=0x58a;
 assert(fvdt_apply(state,&probe,g,&f,&e,true,false,&c,isample,icas,iwrite)==FAT_REJECTED&&!c.events);
 struct fvdt_state*saved=malloc(sizeof*saved);assert(saved);*saved=*state;assert(fvdt_apply(state,&probe,g,&f,&e,false,true,&c,isample,icas,iwrite)==FAT_REJECTED&&!memcmp(state,saved,sizeof*state));free(saved);
 for(unsigned i=0;i<132;i++)assert(!memcmp(h->bytes+(m->flash[i]-BASE),vars+(size_t)i*4096,4096));
 assert(fwm_release(m)&&!h->live);free(state);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("VARS dirty integration: final stage publish, sample/CAS/RF/RIP fatal retention, one-shot PASS");return 0;
}
