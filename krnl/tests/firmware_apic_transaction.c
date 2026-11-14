#define _POSIX_C_SOURCE 200112L
#include "../firmware_apic_transaction.h"
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
static void reject(struct fat_state*s,uint64_t*g,struct fmm_memory*f,struct fat_exit*e,uint32_t ecx,uint32_t edx,bool exclusive){
 struct fat_state old=*s;uint64_t saved[15];memcpy(saved,g,sizeof saved);struct writes w={0};
 assert(fat_mmio(s,g,f,e,ecx,edx,exclusive,&w,wr)==FAT_REJECTED);
 assert(!w.calls&&!memcmp(s,&old,sizeof old)&&!memcmp(g,saved,sizeof saved));
}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536]={0},vars[4096]={0};
 struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={m,0x80000033,0x1000,0x2660,0x500,0,0xa09b,8,false};
 put(m,0x1000,0x2023);put(m,0x2018,0x4023);put(m,0x4000+511*8,0x5023);
 put(m,0x5000+511*8,0xfffff063);put(m,0x4000+503*8,0x6023);put(m,0x6000,0xfee00063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;
 struct fat_state s={0};assert(fag_init(&s.device));uint64_t g[15]={0};
 struct fat_exit e={.rip=0xfffffff0,.rsp=0x12345000,.qualification=0x181,.linear=0xfee00030,.physical=0xfee00030,.reason=48,.dr7=0x400};
 /* mov eax,[rbx]: APIC version read zero-extends and commits RIP. */
 rom[0]=0x8b;rom[1]=0x03;g[1]=e.linear;g[0]=UINT64_MAX;
 struct writes w={0};uint32_t version=0;assert(fag_mmio(&s.device,0x30,4,false,&version));
 assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_COMMITTED);
 assert(g[0]==version&&w.calls==1&&w.fields[0]==0x681e&&w.values[0]==e.rip+2);
 /* Architectural RCX maps to native slot 2 (not native RBX slot 1). */
 rom[1]=0x0b;g[2]=UINT64_MAX;w=(struct writes){0};
 assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_COMMITTED&&g[2]==version&&g[1]==e.linear);
 rom[1]=0x03;
 /* mov [rbx],eax: staged TPR write. */
 rom[0]=0x89;g[0]=0x70;g[1]=e.linear=e.physical=0xfee00080;e.qualification=0x182;w=(struct writes){0};
 assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_COMMITTED);
 uint32_t value=0;assert(fag_mmio(&s.device,0x80,4,false,&value)&&value==0x70);
 /* RIP failure must not publish the staged device write. */
 struct fat_state saved=s;g[0]=0x20;uint64_t savedg[15];memcpy(savedg,g,sizeof g);w=(struct writes){.fail=1};
 assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_FATAL&&s.failed);
 assert(!memcmp(&s.device,&saved.device,sizeof s.device)&&!memcmp(g,savedg,sizeof g));
 reject(&s,g,&f,&e,0,1u<<9,true);s=saved; /* Test fixture only; production never clears latch. */
 /* mov esp,[rbx], then fail second VMWRITE: RSP may have changed in VMCS,
  * but no GPR/device commit and latch prevents all retry/resume. */
 rom[0]=0x8b;rom[1]=0x23;g[1]=e.linear=e.physical=0xfee00030;e.qualification=0x181;
 memcpy(savedg,g,sizeof g);saved=s;w=(struct writes){.fail=2};
 assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_FATAL&&s.failed);
 assert(w.calls==2&&w.fields[0]==0x681c&&w.values[0]==version&&w.fields[1]==0x681e);
 assert(!memcmp(&s.device,&saved.device,sizeof s.device)&&!memcmp(g,savedg,sizeof g));reject(&s,g,&f,&e,0,1u<<9,true);
 s=saved;w=(struct writes){.fail=1};assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_FATAL&&w.calls==1);
 s=saved;w=(struct writes){0};assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_COMMITTED&&w.calls==2);
 /* EPT RF=1 belongs to the restarted MOV. Successful completion clears only
  * RF, with RSP -> RFLAGS -> RIP ordering and fail-stop on every write. */
 e.rflags=0x10093;saved=s;memcpy(savedg,g,sizeof g);
 for(unsigned failure=1;failure<=3;failure++){
  s=saved;memcpy(g,savedg,sizeof g);w=(struct writes){.fail=failure};
  assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_FATAL&&s.failed&&w.calls==failure);
  assert(!memcmp(&s.device,&saved.device,sizeof s.device)&&!memcmp(g,savedg,sizeof g));
  reject(&s,g,&f,&e,0,1u<<9,true);
 }
 s=saved;w=(struct writes){0};assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_COMMITTED);
 assert(w.calls==3&&w.fields[0]==0x681c&&w.fields[1]==0x6820&&w.values[1]==0x93&&w.fields[2]==0x681e);
 rom[1]=0x03;w=(struct writes){0};assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_COMMITTED);
 assert(w.calls==2&&w.fields[0]==0x6820&&w.values[0]==0x93&&w.fields[1]==0x681e);
 /* RF=1 store must not publish a staged device update on either failure. */
 rom[0]=0x89;g[1]=e.linear=e.physical=0xfee00080;e.qualification=0x182;g[0]=0x10;saved=s;memcpy(savedg,g,sizeof g);
 for(unsigned failure=1;failure<=2;failure++){
  s=saved;w=(struct writes){.fail=failure};
  assert(fat_mmio(&s,g,&f,&e,0,1u<<9,true,&w,wr)==FAT_FATAL&&s.failed);
  assert(!memcmp(&s.device,&saved.device,sizeof s.device)&&!memcmp(g,savedg,sizeof g));
 }
 s=saved;rom[0]=0x8b;g[1]=e.linear=e.physical=0xfee00030;e.qualification=0x181;
 rom[0]=0x90;reject(&s,g,&f,&e,0,1u<<9,true);rom[0]=0x8b;
 e.dr7=0x401;reject(&s,g,&f,&e,0,1u<<9,true);e.dr7=0x402;reject(&s,g,&f,&e,0,1u<<9,true);e.dr7=0x2400;reject(&s,g,&f,&e,0,1u<<9,true);e.dr7=0x400;e.rflags=0;
 /* Admission failures perform no callback nor mutation. */
 reject(&s,g,&f,&e,0,0,true);reject(&s,g,&f,&e,1u<<21,1u<<9,true);reject(&s,g,&f,&e,0,1u<<9,false);
 e.entry_event=1;reject(&s,g,&f,&e,0,1u<<9,true);e.entry_event=0;
 e.rflags=1u<<8;reject(&s,g,&f,&e,0,1u<<9,true);e.rflags=(1u<<16)|(1u<<8);reject(&s,g,&f,&e,0,1u<<9,true);e.rflags=0;
 e.blocking=1;reject(&s,g,&f,&e,0,1u<<9,true);e.blocking=2;reject(&s,g,&f,&e,0,1u<<9,true);e.blocking=0;
 e.pending_debug=1;reject(&s,g,&f,&e,0,1u<<9,true);e.pending_debug=0;
 e.vectoring=1;reject(&s,g,&f,&e,0,1u<<9,true);e.vectoring=0;
 uint64_t*bases[]={&e.ds_base,&e.es_base,&e.ss_base};for(unsigned i=0;i<3;i++){*bases[i]=1;reject(&s,g,&f,&e,0,1u<<9,true);*bases[i]=0;}
 e.reason=49;reject(&s,g,&f,&e,0,1u<<9,true);e.reason=48;
 e.qualification=0x183;reject(&s,g,&f,&e,0,1u<<9,true);e.qualification=0x181;
 rom[0]=0x8a;reject(&s,g,&f,&e,0,1u<<9,true); /* byte MOV is unsupported device width */
 rom[0]=0x90;reject(&s,g,&f,&e,0,1u<<9,true);rom[0]=0x8b;
 put(m,0x1000,0x2003);reject(&s,g,&f,&e,0,1u<<9,true);put(m,0x1000,0x2023);
 g[1]=e.linear=e.physical=0xfee00040;reject(&s,g,&f,&e,0,1u<<9,true); /* unsupported register */
 rom[0]=0x89;rom[1]=0x03;e.qualification=0x182;
 unsigned offsets[]={0x300,0x310,0x3f0,0x380};
 for(unsigned i=0;i<4;i++){g[1]=e.linear=e.physical=0xfee00000+offsets[i];g[0]=0x41;reject(&s,g,&f,&e,0,1u<<9,true);}
 /* Exact fixed-ROM opcode is MOV [RBX],ESI. Telemetry must report RSI,
  * not the unrelated RAX snapshot, with staged idle-only register commit. */
 rom[0]=0x89;rom[1]=0x33;g[4]=0x20005;g[0]=0xfee00000;
 g[1]=e.linear=e.physical=0xfee00320;e.qualification=0x182;e.rflags=0x10093;
 saved=s;memcpy(savedg,g,sizeof g);w=(struct writes){0};struct fat_trace trace;
 for(unsigned fail=1;fail<=2;fail++) {
  struct fat_state candidate=saved;struct writes bad={.fail=fail};
  assert(fat_mmio(&candidate,g,&f,&e,0,1u<<9,true,&bad,wr)==FAT_FATAL && candidate.failed);
  assert(!memcmp(&candidate.device,&saved.device,sizeof candidate.device));
  assert(!memcmp(g,savedg,sizeof g));
 }
 assert(fat_mmio_traced(&s,g,&f,&e,0,1u<<9,true,&w,wr,&trace)==FAT_COMMITTED);
 assert(trace.stage==6&&trace.width==4&&trace.reg==6&&trace.write&&trace.store_value==0x20005);
 assert(trace.lvt==saved.device.apic.timer.lvt&&!trace.armed&&!trace.initial);
 assert(s.device.apic.timer.lvt==0x20005 && fag_valid(&s.device));
 assert(w.calls==2&&!memcmp(g,savedg,sizeof g));
 g[1]=e.linear=e.physical=0xfee00380;g[4]=0xffffffff;reject(&s,g,&f,&e,0,1u<<9,true);
 assert(fwm_release(m)&&!h->live);free(h->bytes);free(h);free(m);
 puts("firmware APIC transaction: ROM MOV, staged commit, fatal VMWRITE, admission gates PASS");return 0;
}
