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


struct writes {unsigned calls,fail;uint32_t fields[2];uint64_t values[2];};
static bool wr(void*x,uint32_t field,uint64_t value){struct writes*w=x;assert(w->calls<2);w->fields[w->calls]=field;w->values[w->calls++]=value;return w->calls!=w->fail;}

static void no(struct fvr_state*s,uint64_t*g,struct fmm_memory*f,struct fat_exit*e){struct fvr_state old=*s;uint64_t saved[15];memcpy(saved,g,sizeof saved);struct writes w={0};assert(fvr_mmio(s,g,f,e,true,&w,wr)==FAT_REJECTED&&!w.calls&&!memcmp(&old,s,sizeof old)&&!memcmp(saved,g,sizeof saved));}
static bool valid(struct fmm_memory*f,struct fat_exit*e){return fmm_validate_vars_write(f,e->linear,1,e->reason,e->qualification,e->linear,e->physical,e->vectoring);}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);vars[16]=0x8d;struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x1000,0x2023);put(m,0x2018,0x4023);put(m,0x4000+511*8,0x5023);put(m,0x5000+511*8,0xfffff063);
 put(m,0x4000+510*8,0x6023);put(m,0x6000,0xffc00063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0x88;rom[1]=0x03;
 uint8_t*back=h->bytes+(m->flash[0]-BASE);struct fvr_state s={0};assert(fvr_init(&s,m)&&s.address==0xffc00010&&s.original==0x8d&&s.stage==0);
 struct fvr_state fresh=s;uint64_t g[15]={0};g[0]=0x50;g[1]=s.address;
 struct fat_exit e={.rip=0xfffffff0,.qualification=0x58a,.linear=s.address,.physical=s.address,.rflags=0x10002,.dr7=0x400,.reason=48};
 assert(valid(&f,&e));assert(!fmm_validate(&f,e.linear,1,true,48,e.qualification,e.linear,e.physical,0,e.physical,1));
 struct writes w={0};assert(fvr_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED&&s.stage==1&&back[16]==0x8d&&g[0]==0x50&&w.calls==2&&w.fields[0]==0x6820&&w.values[0]==2&&w.fields[1]==0x681e);
 g[0]=0x70;w=(struct writes){0};assert(fvr_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED&&s.stage==2&&back[16]==0x8d);no(&s,g,&f,&e);
 for(unsigned i=0;i<132;i++)assert(!memcmp(h->bytes+(m->flash[i]-BASE),vars+(size_t)i*4096,4096));
 for(unsigned fail=1;fail<=2;fail++){s=fresh;g[0]=0x50;w=(struct writes){.fail=fail};assert(fvr_mmio(&s,g,&f,&e,true,&w,wr)==FAT_FATAL&&s.failed&&!s.stage&&w.calls==fail&&back[16]==0x8d);no(&s,g,&f,&e);}s=fresh;
 /* Probe only accepts50 then70; byte commands never mutate backing. */
 /* Observed c6 03 50 and exact RF flags: diagnostic path stays identical. */
 rom[0]=0xc6;rom[1]=0x03;rom[2]=0x50;e.rflags=0x10046;
 struct fvr_trace trace;s=fresh;w=(struct writes){0};
 assert(fvr_mmio_traced(&s,g,&f,&e,true,&w,wr,&trace)==FAT_COMMITTED&&trace.stage==6&&trace.width==1&&trace.write&&trace.value==0x50&&trace.linear==s.address&&w.values[0]==0x46);
 put(m,0x6000,0xffc00023);s=fresh;w=(struct writes){0};
 assert(fvr_mmio_traced(&s,g,&f,&e,true,&w,wr,&trace)==FAT_REJECTED&&trace.stage==3&&!w.calls&&!s.stage&&!s.failed);
 put(m,0x6000,0xffc00063);rom[0]=0x88;rom[1]=0x03;e.rflags=0x10002;
 for(unsigned value=0;value<256;value++)if(value!=0x50){struct fvr_state temp=fresh;assert(!fvr_probe(&temp,m,temp.address,1,value)&&!memcmp(&temp,&fresh,sizeof temp));}
 struct fvr_state temp=fresh;assert(!fvr_probe(&temp,m,temp.address,2,0x50));assert(!fvr_probe(&temp,m,temp.address+1,1,0x50));
 back[16]=0x8e;assert(!fvr_probe(&temp,m,temp.address,1,0x50));back[16]=0x8d;
 /* Strict EPT readable-ROM evidence; hole qualifiers must not pass. */
 uint64_t qs[]={0x182,0x582,0x581,0x18e,0x59a,0x78a,0x18b,UINT64_MAX};
 for(unsigned i=0;i<sizeof qs/sizeof*qs;i++){e.qualification=qs[i];assert(!valid(&f,&e));}e.qualification=0x58a;
 f.advanced_ept_info=false;assert(!valid(&f,&e));e.qualification=0x18a;assert(valid(&f,&e));f.advanced_ept_info=true;assert(!valid(&f,&e));e.qualification=0x58a;
 put(m,0x6000,0xffc00061);assert(!valid(&f,&e));put(m,0x6000,0xffc00023);assert(!valid(&f,&e));put(m,0x6000,0xffc00043);assert(!valid(&f,&e));put(m,0x6000,0xffc00063);
 uint64_t*leaf=(uint64_t*)(h->bytes+(m->tables[68]-BASE));uint64_t oldleaf=leaf[0];
 unsigned perms[]={0,1,0x11,0x33,0x35,0x37,0x71};for(unsigned i=0;i<sizeof perms/sizeof*perms;i++){leaf[0]=m->flash[0]|perms[i];assert(!valid(&f,&e));}leaf[0]=oldleaf;
 unsigned slots[]={0,1,3},indices[]={0,3,510};for(unsigned i=0;i<3;i++){uint64_t*p=(uint64_t*)(h->bytes+(m->tables[slots[i]]-BASE));uint64_t old=p[indices[i]];p[indices[i]]=old&~2ULL;assert(!valid(&f,&e));p[indices[i]]=old;}
 uint64_t old=m->ram[8];m->ram[8]=m->flash[0];assert(!valid(&f,&e));m->ram[8]=old;old=m->flash[1];m->flash[1]=m->flash[0];assert(!valid(&f,&e));m->flash[1]=old;
 old=m->tables[4];m->tables[4]=m->flash[0];assert(!valid(&f,&e));m->tables[4]=old;
 old=m->flash[0];m->flash[0]=0xdead000;assert(!valid(&f,&e));m->flash[0]=old;
 m->layout.vars.write=true;assert(!valid(&f,&e));m->layout.vars.write=false;m->layout.vars.execute=true;assert(!valid(&f,&e));m->layout.vars.execute=false;
 for(unsigned i=1;i<=8;i++){h->fail_map=h->maps+i;assert(!valid(&f,&e));h->fail_map=0;}
 g[0]=0x50;rom[0]=0x89;no(&s,g,&f,&e);rom[0]=0x8a;no(&s,g,&f,&e);rom[0]=0x88;
 e.linear++;no(&s,g,&f,&e);e.linear--;e.physical++;no(&s,g,&f,&e);e.physical--;
 e.rflags=0x102;no(&s,g,&f,&e);e.rflags=0x10002;e.blocking=1;no(&s,g,&f,&e);e.blocking=0;e.entry_event=1;no(&s,g,&f,&e);e.entry_event=0;e.vectoring=1;no(&s,g,&f,&e);e.vectoring=0;
 /* First64KiB boundary, not just first4KiB; absent candidate rejects. */
 back[16]=0;uint8_t*last=h->bytes+(m->flash[15]-BASE);last[4095]=0x8d;temp=(struct fvr_state){0};assert(fvr_init(&temp,m)&&temp.address==0xffc0ffff);last[4095]=0;
 temp=(struct fvr_state){0};assert(!fvr_init(&temp,m)&&!temp.ready);back[16]=0x8d;
 assert(fwm_release(m)&&!h->live);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("VARS ROM: probe50/70 unchanged backing, strict read-only EPT/guest permissions, fatal commit PASS");return 0;
}
