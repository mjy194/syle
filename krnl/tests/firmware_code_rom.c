#define _POSIX_C_SOURCE 200112L
#include "../firmware_code_rom.h"
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


struct writes {unsigned calls,fail;uint32_t fields[2];uint64_t values[2],rip,flags;};
static bool wr(void*x,uint32_t field,uint64_t value){
 struct writes*w=x;assert(w->calls<2);w->fields[w->calls]=field;w->values[w->calls++]=value;
 if(field==0x681e)w->rip=value;else if(field==0x6820)w->flags=value;else assert(false);
 return w->calls!=w->fail; /* A failed callback may already have written VMCS. */
}
static void reject(struct fcode_state*s,uint64_t*g,struct fmm_memory*f,struct fat_exit*e){
 struct fcode_state old=*s;uint64_t saved[15];memcpy(saved,g,sizeof saved);struct writes w={0};
 assert(fcode_mmio(s,g,f,e,true,&w,wr)==FAT_REJECTED&&!w.calls);
 assert(!memcmp(&old,s,sizeof old)&&!memcmp(saved,g,sizeof saved));
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);code[16]=0x78;vars[16]=0x8d;
 struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2668,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0xe3);
 put(m,0x2018,0x4023);put(m,0x4000+510*8,0xffc000e3);
 uint8_t insn[]={0xc6,0x00,0x50};assert(fwm_write_ram(m,0x7000,insn,sizeof insn));
 struct fcode_state s={0};assert(fcode_init(&s,m)&&s.address==0xffc84010&&s.original==0x78&&s.stage==0);
 assert(!fcode_init(&s,m));struct fcode_state fresh=s;
 uint64_t g[15]={0};g[0]=s.address;uint64_t saved[15];memcpy(saved,g,sizeof saved);
 struct fat_exit e={.rip=0x7000,.qualification=0x5aa,.linear=s.address,.physical=s.address,.rflags=0x10206,.dr7=0x400,.reason=48};
 size_t total=(size_t)COUNT*4096;uint8_t*before=malloc(total);assert(before);memcpy(before,h->bytes,total);
 struct writes w={0};assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED);
 assert(s.stage==1&&!s.failed&&w.calls==2&&w.fields[0]==0x6820&&w.values[0]==0x206&&w.fields[1]==0x681e&&w.rip==0x7003);
 assert(!memcmp(saved,g,sizeof saved)&&!memcmp(before,h->bytes,total));
 reject(&s,g,&f,&e); /* A second50 is not a new probe. */
 uint8_t*v=h->bytes+(m->ram[7]-BASE);v[2]=0x70;w=(struct writes){0};
 assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED&&s.stage==2);reject(&s,g,&f,&e);
 s=fresh;reject(&s,g,&f,&e);v[2]=0x50;
 for(unsigned fail=1;fail<=2;fail++){
  s=fresh;w=(struct writes){.fail=fail};
  assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_FATAL&&s.failed&&!s.stage&&w.calls==fail);
  reject(&s,g,&f,&e);assert(!memcmp(before,h->bytes,total));
 }
 s=fresh;e.rflags=0x206;w=(struct writes){0};
 assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED&&w.calls==1&&w.fields[0]==0x681e&&w.rip==0x7003);
 s=fresh;w=(struct writes){.fail=1};assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_FATAL&&s.failed&&!s.stage);
 s=fresh;e.rflags=0x10206;
 /* Supported register MOV uses the same target/value/commit checks. */
 v[0]=0x88;v[1]=0x03;g[0]=0x50;g[1]=s.address;w=(struct writes){0};
 assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED&&w.rip==0x7002&&g[0]==0x50&&g[1]==s.address);
 s=fresh;g[0]=s.address;g[1]=0;v[0]=0xc6;v[1]=0;
 uint64_t quals[]={0x182,0x582,0x18a,0x58a,0x1ab,0x1ae,0x7aa,0x5ab,UINT64_MAX};
 for(unsigned i=0;i<sizeof quals/sizeof*quals;i++){e.qualification=quals[i];reject(&s,g,&f,&e);}e.qualification=0x5aa;
 f.advanced_ept_info=false;reject(&s,g,&f,&e);e.qualification=0x1aa;w=(struct writes){0};
 assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED);s=fresh;f.advanced_ept_info=true;reject(&s,g,&f,&e);e.qualification=0x5aa;
 assert(!fmm_validate_vars_write(&f,e.linear,1,48,e.qualification,e.linear,e.physical,0));
 assert(!fmm_validate(&f,e.linear,1,true,48,e.qualification,e.linear,e.physical,0,0xffc84000,4096));
 uint64_t badpt[]={0xffc000c3,0xffc000a3,0xffc000e1,0xffc000e7};
 for(unsigned i=0;i<sizeof badpt/sizeof*badpt;i++){
  put(m,0x4000+510*8,badpt[i]);
  if(i==3){put(m,0x1000,0x2027);put(m,0x2018,0x4027);}
  reject(&s,g,&f,&e);put(m,0x1000,0x2023);put(m,0x2018,0x4023);
 }put(m,0x4000+510*8,0xffc000e3);
 /* 4KiB guest leaves obey the same admission as the observed2MiB leaf. */
 put(m,0x4000+510*8,0x6023);put(m,0x6000+132*8,0xffc84063);w=(struct writes){0};
 assert(fcode_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED);s=fresh;
 put(m,0x6000+132*8,0xffc85063);reject(&s,g,&f,&e);put(m,0x6000+132*8,0);put(m,0x4000+510*8,0xffc000e3);
 uint64_t*leaf=(uint64_t*)(h->bytes+(m->tables[68]-BASE));uint64_t old=leaf[132];
 unsigned perms[]={0,1,0x31,0x33,0x37,0x75};
 for(unsigned i=0;i<sizeof perms/sizeof*perms;i++){leaf[132]=m->flash[132]|perms[i];reject(&s,g,&f,&e);}leaf[132]=old;
 unsigned slots[]={0,1,3},indices[]={0,3,510};
 for(unsigned i=0;i<3;i++){uint64_t*p=(uint64_t*)(h->bytes+(m->tables[slots[i]]-BASE));old=p[indices[i]];p[indices[i]]=old&~2ULL;reject(&s,g,&f,&e);p[indices[i]]=old;}
 old=m->ram[8];m->ram[8]=m->flash[132];reject(&s,g,&f,&e);m->ram[8]=old;
 old=m->flash[133];m->flash[133]=m->flash[132];reject(&s,g,&f,&e);m->flash[133]=old;
 old=m->tables[4];m->tables[4]=m->flash[132];reject(&s,g,&f,&e);m->tables[4]=old;
 uint8_t*back=h->bytes+(m->flash[132]-BASE);back[16]=0x79;reject(&s,g,&f,&e);back[16]=0x78;
 m->layout.code.write=true;reject(&s,g,&f,&e);m->layout.code.write=false;
 m->layout.code.execute=false;reject(&s,g,&f,&e);m->layout.code.execute=true;
 e.linear++;reject(&s,g,&f,&e);e.linear--;e.physical++;reject(&s,g,&f,&e);e.physical--;
 e.physical=0xffc00010;e.linear=e.physical;g[0]=e.physical;reject(&s,g,&f,&e);e.physical=s.address;e.linear=s.address;g[0]=s.address;
 e.reason=0x80000030;reject(&s,g,&f,&e);e.reason=48;
 e.vectoring=1;reject(&s,g,&f,&e);e.vectoring=0;e.entry_event=1;reject(&s,g,&f,&e);e.entry_event=0;
 e.blocking=1;reject(&s,g,&f,&e);e.blocking=0;e.pending_debug=1;reject(&s,g,&f,&e);e.pending_debug=0;
 e.dr7=0x401;reject(&s,g,&f,&e);e.dr7=0x400;e.rflags|=0x100;reject(&s,g,&f,&e);e.rflags=0x10206;
 e.ds_base=1;reject(&s,g,&f,&e);e.ds_base=0;
 w=(struct writes){0};assert(fcode_mmio(&s,g,&f,&e,false,&w,wr)==FAT_REJECTED&&!w.calls);
 for(unsigned value=0;value<256;value++)if(value!=0x50){v[2]=(uint8_t)value;reject(&s,g,&f,&e);}
 v[2]=0x50;
 v[0]=0x8a;reject(&s,g,&f,&e);v[0]=0xc7;reject(&s,g,&f,&e);v[0]=0xf0;reject(&s,g,&f,&e);v[0]=0xc6;
 /* Truncated instruction at end of mapped RAM must not commit. */
 uint8_t last=0xc6;assert(fwm_write_ram(m,FWM_RAM_SIZE-1,&last,1));put(m,0x3000+63*8,0x7e000e3);
 e.rip=FWM_RAM_SIZE-1;reject(&s,g,&f,&e);e.rip=0x7000;last=0;assert(fwm_write_ram(m,FWM_RAM_SIZE-1,&last,1));put(m,0x3000+63*8,0);
 for(unsigned i=1;i<=8;i++){h->fail_map=h->maps+i;reject(&s,g,&f,&e);h->fail_map=0;}
 /* Candidate search is bounded to first64KiB, including its final byte. */
 back[16]=0;uint8_t*end=h->bytes+(m->flash[147]-BASE);end[4095]=0x7b;
 struct fcode_state other={0};assert(fcode_init(&other,m)&&other.address==0xffc93fff&&other.original==0x7b);
 end[4095]=0;other=(struct fcode_state){0};uint8_t*out=h->bytes+(m->flash[148]-BASE);out[0]=0x7b;
 assert(!fcode_init(&other,m)&&!other.ready);out[0]=0;back[16]=0x78;
 assert(!memcmp(before,h->bytes,total)&&!memcmp(saved,g,sizeof saved));
 assert(fwm_release(m)&&!h->live);free(before);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("CODE ROM: immutable50/70, RX ownership, guest permissions, RF/RIP and fatal partial commit PASS");
}
