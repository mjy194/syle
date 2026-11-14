#define _POSIX_C_SOURCE 200112L
#include "../firmware_tpm_transaction.h"
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
static void no(struct ftt_state*s,uint64_t*g,struct fmm_memory*f,struct fat_exit*e,bool exclusive){struct ftt_state old=*s;uint64_t saved[15];memcpy(saved,g,sizeof saved);struct writes w={0};assert(ftt_mmio(s,g,f,e,exclusive,&w,wr)==FAT_REJECTED&&!w.calls&&!memcmp(&old,s,sizeof old)&&!memcmp(saved,g,sizeof saved));}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536]={0},vars[4096]={0};struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={.memory=m,.cr0=0x80000033,.cr3=0x1000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x1000,0x2023);put(m,0x2018,0x4023);put(m,0x4000+511*8,0x5023);put(m,0x5000+511*8,0xfffff063);
 put(m,0x4000+502*8,0x6023);put(m,0x6000+320*8,0xfed40063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0x8a;rom[1]=0x03; /* mov al,[rbx] */
 struct ftt_state s={0};uint64_t g[15]={0};g[0]=0x123456789abcde12;g[1]=0xfed40000;
 struct fat_exit e={.rip=0xfffffff0,.rsp=0x80000,.qualification=0x581,.linear=0xfed40000,.physical=0xfed40000,.rflags=0x10002,.dr7=0x400,.reason=48};
 struct writes w={0};assert(ftt_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED&&g[0]==0x123456789abcdeff&&w.calls==2&&w.fields[0]==0x6820&&w.values[0]==2&&w.fields[1]==0x681e&&w.values[1]==e.rip+2);
 for(unsigned fail=1;fail<=2;fail++){s=(struct ftt_state){0};g[0]=0x123456789abcde12;uint64_t old[15];memcpy(old,g,sizeof old);w=(struct writes){.fail=fail};assert(ftt_mmio(&s,g,&f,&e,true,&w,wr)==FAT_FATAL&&s.failed&&w.calls==fail&&!memcmp(old,g,sizeof old));no(&s,g,&f,&e,true);}s=(struct ftt_state){0};
 /* No RF: RIP only; AH merge remains distinct from SPL, which rejects. */
 e.rflags=2;rom[1]=0x23;w=(struct writes){0};g[0]=0x123456789abc0012;
 assert(ftt_mmio(&s,g,&f,&e,true,&w,wr)==FAT_COMMITTED&&g[0]==0x123456789abcff12&&w.calls==1&&w.fields[0]==0x681e);
 rom[0]=0x40;rom[1]=0x8a;rom[2]=0x23;no(&s,g,&f,&e,true);rom[0]=0x8a;rom[1]=0x03;
 no(&s,g,&f,&e,false);
 for(unsigned bit=0;bit<64;bit++)if(!(UINT64_C(0x250ed7)&(UINT64_C(1)<<bit))){e.rflags=2|(UINT64_C(1)<<bit);no(&s,g,&f,&e,true);}
 e.rflags=0;no(&s,g,&f,&e,true);
 e.rflags=0x102;no(&s,g,&f,&e,true);e.rflags=2;e.dr7=0x401;no(&s,g,&f,&e,true);e.dr7=0x400;
 e.pending_debug=1;no(&s,g,&f,&e,true);e.pending_debug=0;e.blocking=1;no(&s,g,&f,&e,true);e.blocking=0;e.entry_event=1;no(&s,g,&f,&e,true);e.entry_event=0;e.vectoring=1;no(&s,g,&f,&e,true);e.vectoring=0;
 uint64_t*bases[]={&e.ds_base,&e.es_base,&e.ss_base};for(unsigned i=0;i<3;i++){*bases[i]=1;no(&s,g,&f,&e,true);*bases[i]=0;}
 rom[0]=0x8b;no(&s,g,&f,&e,true);rom[0]=0x88;e.qualification=0x582;no(&s,g,&f,&e,true);rom[0]=0x90;no(&s,g,&f,&e,true);rom[0]=0x8a;e.qualification=0x581;
 g[1]=e.linear=e.physical=0xfed40001;no(&s,g,&f,&e,true);g[1]=e.linear=e.physical=0xfed40000;
 e.linear++;no(&s,g,&f,&e,true);e.linear--;e.physical++;no(&s,g,&f,&e,true);e.physical--;
 put(m,0x6000+320*8,0xfed41063);no(&s,g,&f,&e,true);put(m,0x6000+320*8,0xfed40063);
 put(m,0x1000,0x2003);no(&s,g,&f,&e,true);put(m,0x1000,0x2023);
 f.advanced_ept_info=false;no(&s,g,&f,&e,true);f.advanced_ept_info=true;f.cs_selector=3;no(&s,g,&f,&e,true);f.cs_selector=8;
 h->fail_map=h->maps+1;no(&s,g,&f,&e,true);h->fail_map=0;
 assert(fwm_release(m)&&!h->live);free(h->bytes);free(h);free(m);
 puts("TPM absent transaction: owned ROM AL/AH, RF-before-RIP, partial-write fatal, strict evidence PASS");return 0;
}
