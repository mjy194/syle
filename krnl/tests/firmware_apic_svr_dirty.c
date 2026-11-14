#define _POSIX_C_SOURCE 200112L
#include "../firmware_apic_svr_dirty_transaction.h"
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





struct integration {struct firmware_memory*m;struct fct_state*a;unsigned samples,cas,writes,fail_sample,fail_write;bool uncertain,conflict,ancestor_change,device_change;};
static bool sample(void*x){struct integration*c=x;c->samples++;assert(c->a->device.apic.irq.svr!=0x10f);c->a->reference++;if(c->ancestor_change&&c->samples==1)put(c->m,0x7c01000,0x7c02033);if(c->device_change&&c->samples==2)c->a->device.apic.irq.tpr=0x10;return ft100_sample(&c->a->clock.timer,c->a->epoch,c->a->reference,0)==FT100_OK&&c->samples!=c->fail_sample;}
static enum fwd_cas_result cas(void*x,uint64_t*p,uint64_t old,uint64_t desired){struct integration*c=x;c->cas++;assert(!(old&0x40)&&(old&0x20)&&desired==(old|0x40));if(c->uncertain){*p=desired;return FWD_CAS_UNCERTAIN;}if(c->conflict)*p=old^0x10;return fwd_atomic_cas(p,old,desired);}
static bool writevm(void*x,uint32_t field,uint64_t value){struct integration*c=x;c->writes++;assert(c->a->device.apic.irq.svr!=0x10f);assert((field==0x6820&&value==0x46)||(field==0x681e&&value==0xfffffff4));return c->writes!=c->fail_write;}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);vars[16]=0x8d;struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06fb8,0xfee000a3);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0x41;rom[1]=0x89;rom[2]=0x1c;rom[3]=0x24;
 struct fct_state active={0};assert(fag_init(&active.device)&&ft100_mmio_init(&active.clock,7,100,1000));active.epoch=7;active.reference=100;
 uint64_t g[15]={0};g[0]=UINT64_MAX;g[1]=0x10f;g[11]=0xfee000f0;
 struct fat_exit e={.rip=0xfffffff0,.qualification=0x582,.linear=0xfee000f0,.physical=0xfee000f0,.rflags=0x10046,.dr7=0x400,.reason=48};

 struct fsd_plan plan;assert(fsd_capture(&f,&e,g,&active,0,1u<<9,&plan)&&plan.transaction.leaf_entry_gpa==0x7c06fb8&&plan.transaction.leaf_value==0xfee000a3&&fsd_validate(&plan,false));
 assert(!fmm_validate(&f,e.linear,4,true,48,0x582,e.linear,e.physical,0,0xfee000f0,4));
 struct fsd_plan tampered=plan;tampered.staged_device.apic.lvt[0]^=1;assert(!fsd_validate(&tampered,false));tampered=plan;tampered.staged_device.apic.ldr^=0x1000000;assert(!fsd_validate(&tampered,false));tampered=plan;tampered.staged_device.apic.irq.irr[0]=1;assert(!fsd_validate(&tampered,false));tampered=plan;tampered.staged_device.apic.timer.divide^=1;assert(!fsd_validate(&tampered,false));
 struct fsdt_state*state=calloc(1,sizeof*state);assert(state);struct integration c={.m=m,.a=&active};struct fct_state pristine=active;
 uint64_t before[15];memcpy(before,g,sizeof g);uint16_t original_svr=active.device.apic.irq.svr;
 assert(fsdt_apply(state,&active,g,&f,&e,0,1u<<9,true,true,&c,sample,cas,writevm)==FAT_COMMITTED&&state->complete&&state->dirty_known&&!memcmp(g,before,sizeof g)&&active.device.apic.irq.svr==0x10f&&c.cas==1&&c.samples==2&&c.writes==2&&active.clock.timer.last==102);
 assert(active.device.apic.ldr==pristine.device.apic.ldr&&active.device.apic.dfr==pristine.device.apic.dfr&&active.device.apic.irq.tpr==pristine.device.apic.irq.tpr&&active.device.apic.icr==pristine.device.apic.icr);
 for(unsigned i=0;i<6;i++)assert(active.device.apic.lvt[i]==pristine.device.apic.lvt[i]);
 for(unsigned i=0;i<8;i++)assert(active.device.apic.irq.irr[i]==pristine.device.apic.irq.irr[i]&&active.device.apic.irq.isr[i]==pristine.device.apic.irq.isr[i]);
 for(unsigned i=0;i<15;i++)assert(g[i]==before[i]);
 uint64_t leaf;assert(fwm_read_ram(m,0x7c06fb8,&leaf,8)&&leaf==0xfee000e3&&(leaf&0x20));
 assert(fmm_validate(&f,e.linear,4,true,48,0x582,e.linear,e.physical,0,0xfee000f0,4));unsigned calls=c.cas;
 assert(fsdt_apply(state,&active,g,&f,&e,0,1u<<9,true,true,&c,sample,cas,writevm)==FAT_REJECTED&&c.cas==calls);
 for(unsigned fault=0;fault<8;fault++){
  active=pristine;memset(state,0,sizeof*state);memcpy(g,before,sizeof g);put(m,0x7c06fb8,0xfee000a3);put(m,0x7c01000,0x7c02023);c=(struct integration){.m=m,.a=&active};
  if(fault==0)c.fail_sample=1;
  if(fault==1)c.fail_sample=2;
  if(fault==2)c.uncertain=true;
  if(fault==3)c.conflict=true;
  if(fault==4)c.fail_write=1;
  if(fault==5)c.fail_write=2;
  if(fault==6)c.ancestor_change=true;
  if(fault==7)c.device_change=true;
  assert(fsdt_apply(state,&active,g,&f,&e,0,1u<<9,true,true,&c,sample,cas,writevm)==FAT_FATAL&&state->failed&&active.failed&&active.clock.timer.terminal&&!memcmp(g,before,sizeof g)&&active.device.apic.irq.svr==original_svr);
  assert(fwm_read_ram(m,0x7c06fb8,&leaf,8)&&leaf==(fault==0||fault==6?0xfee000a3:fault==3?0xfee000b3:0xfee000e3)&&(leaf&0x20));
  if(fault==2)assert(!state->dirty_known);
  if(fault==6)assert(!c.cas);
  calls=c.cas;assert(fsdt_apply(state,&active,g,&f,&e,0,1u<<9,true,true,&c,sample,cas,writevm)==FAT_FATAL&&c.cas==calls);
 }
 active=pristine;memset(state,0,sizeof*state);memcpy(g,before,sizeof g);put(m,0x7c06fb8,0xfee000a3);put(m,0x7c01000,0x7c02023);c=(struct integration){.m=m,.a=&active};
 /* A due real timer sample precedes CAS and is never rolled back. */
 active.device.apic.irq.svr=0x1ff;assert(ft100_start(&active.clock.timer,7,100,1,2,0x41)==FT100_OK);
 assert(fsdt_apply(state,&active,g,&f,&e,0,1u<<9,true,true,&c,sample,cas,writevm)==FAT_FATAL&&!c.cas&&active.clock.timer.terminal);active=pristine;
 /* Armed+software disabled is forbidden, never repaired by proposed10f. */
 assert(ft100_start(&active.clock.timer,7,100,100,2,0x10041)==FT100_OK);
 assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));active=pristine;
 /* Valid enabled masked timer progresses without clock snapshot rollback.
  * One-shot expires only on the second sample (deadline20 bus ticks). */
 for(unsigned periodic=0;periodic<2;periodic++){
  active=pristine;active.device.apic.irq.svr=0x1ff;memset(state,0,sizeof*state);put(m,0x7c06fb8,0xfee000a3);c=(struct integration){.m=m,.a=&active};
  assert(ft100_start(&active.clock.timer,7,100,10,2,periodic?0x30041:0x10041)==FT100_OK);
  assert(fsdt_apply(state,&active,g,&f,&e,0,1u<<9,true,true,&c,sample,cas,writevm)==FAT_COMMITTED&&active.device.apic.irq.svr==0x10f&&active.clock.timer.last==102);
  assert(active.clock.timer.armed==!!periodic&&active.clock.timer.deadline_bus==(periodic?40u:20u));
 }
 /* Unmasked event becomes due after CAS: retainedD, oldSVR, no VMCS write. */
 active=pristine;active.device.apic.irq.svr=0x1ff;memset(state,0,sizeof*state);put(m,0x7c06fb8,0xfee000a3);c=(struct integration){.m=m,.a=&active};
 assert(ft100_start(&active.clock.timer,7,100,10,2,0x41)==FT100_OK);
 assert(fsdt_apply(state,&active,g,&f,&e,0,1u<<9,true,true,&c,sample,cas,writevm)==FAT_FATAL&&c.cas==1&&!c.writes&&active.device.apic.irq.svr==0x1ff&&active.clock.timer.terminal);
 active=pristine;put(m,0x7c06fb8,0xfee000a3);
 uint64_t invalid[]={0xfee000ab,0xfee001a3,0xfee000e3,0xfee00083,0xfee000a1,0xfee000a7,0xfee020a3,0xfee000a3ULL|(1ULL<<63),0xfee00023};
 for(unsigned i=0;i<sizeof invalid/sizeof*invalid;i++){put(m,0x7c06fb8,invalid[i]);assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));}put(m,0x7c06fb8,0xfee000a3);
 put(m,0x7c01000,0x7c02003);assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));put(m,0x7c01000,0x7c02023);
 put(m,0x7c02018,0x7c06003);assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));put(m,0x7c02018,0x7c06023);
 uint64_t*pd=(uint64_t*)(h->bytes+(m->tables[3]-BASE));pd[503]=m->tables[68]|7;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));pd[503]=0;
 unsigned pt=4+(0x7c06000>>21),index=(0x7c06000>>12)&511;uint64_t*ep=(uint64_t*)(h->bytes+(m->tables[pt]-BASE));uint64_t saved=ep[index];ep[index]&=~2ULL;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));ep[index]=saved;
 saved=m->ram[1];m->ram[1]=m->ram[0x7c06];assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));m->ram[1]=saved;
 saved=m->flash[1];m->flash[1]=m->ram[0x7c06];assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));m->flash[1]=saved;
 saved=m->tables[4];m->tables[4]=m->ram[0x7c06];assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));m->tables[4]=saved;
 assert(!fsd_capture(&f,&e,g,&active,0,0,&plan)&&!fsd_capture(&f,&e,g,&active,1u<<21,1u<<9,&plan));
 rom[0]=0x8a;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));rom[0]=0x41;rom[1]=0x88;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));rom[1]=0x89;
 assert(fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));put(m,0x7c07ff8,0xfffff073);assert(!fsd_validate(&plan,false));put(m,0x7c07ff8,0xfffff063);
 rom[0]=0x90;assert(!fsd_validate(&plan,false));rom[0]=0x41;
 e.linear++;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));e.linear--;
 e.qualification=0x181;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));e.qualification=0x582;
 f.advanced_ept_info=false;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));f.advanced_ept_info=true;
 for(unsigned test=0;test<7;test++){
  struct fat_exit bad=e;if(test==0)bad.rflags|=0x100;if(test==1)bad.pending_debug=1;if(test==2)bad.blocking=1;if(test==3)bad.vectoring=1;if(test==4)bad.entry_event=1;if(test==5)bad.dr7=0;if(test==6)bad.ds_base=1;
  assert(!fsd_capture(&f,&bad,g,&active,0,1u<<9,&plan));
 }
 struct fmm_memory invalid_memory=f;invalid_memory.memory=0;assert(!fsd_capture(&invalid_memory,&e,g,&active,0,1u<<9,&plan));
 assert(fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));h->fail_map=h->maps+1;assert(!fsd_validate(&plan,false));h->fail_map=0;
 /* Source instruction backed by leaf-storage RAM is forbidden. */
 put(m,0x7c02000,0x7c08023);put(m,0x7c08000,0x7c09023);put(m,0x7c09030,0x7c06063);uint8_t ins[]={0x41,0x89,0x1c,0x24};assert(fwm_write_ram(m,0x7c06000,ins,4));e.rip=0x6000;assert(!fsd_capture(&f,&e,g,&active,0,1u<<9,&plan));e.rip=0xfffffff0;
 assert(fwm_release(m)&&!h->live);free(state);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("APIC SVR dirty: exactD CAS, device-only final publish, live timer evolution and failure gates PASS");return 0;
}
