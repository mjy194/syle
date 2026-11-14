#define _POSIX_C_SOURCE 200112L
#include "../firmware_vars_dirty.h"
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



struct context {struct fmm_memory*f;struct fat_exit*e;uint64_t*g;struct fvr_state*s;struct fvd_plan plan;unsigned cas_count,samples,writes;bool uncertain,mutate_ancestor,fail_write;};
static enum fwd_cas_result backend(void*x,uint64_t*p,uint64_t old,uint64_t desired){struct context*c=x;c->cas_count++;assert(desired==(old|0x40));if(c->uncertain){*p=desired;return FWD_CAS_UNCERTAIN;}return fwd_atomic_cas(p,old,desired);}
static bool cap(void*x,struct fwd_plan*p){struct context*c=x;if(!fvd_capture(c->f,c->e,c->g,c->s,&c->plan))return false;*p=c->plan.transaction;return true;}
static bool val(void*x,const struct fwd_plan*p,bool after){struct context*c=x;assert(p->leaf_entry_gpa==c->plan.transaction.leaf_entry_gpa);return fvd_validate(&c->plan,after);}
static enum fwd_cas_result cas(void*x,uint64_t a,uint64_t old,uint64_t desired){struct context*c=x;assert(a==c->plan.transaction.leaf_entry_gpa&&old==c->plan.transaction.leaf_value&&desired==(old|0x40));return fvd_compare_set(&c->plan,c,backend);}
static bool sample(void*x){struct context*c=x;c->samples++;if(c->mutate_ancestor&&c->samples==1)put(c->f->memory,0x7c01000,0x7c02033);return true;}
static bool writevm(void*x,uint32_t field,uint64_t value){struct context*c=x;c->writes++;assert((field==0x6820&&value==0x46)||(field==0x681e&&value==c->e->rip+3));return !c->fail_write;}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char*code=calloc(1,3653632),*vars=calloc(1,540672);assert(code&&vars);vars[16]=0x8d;struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,3653632,vars,540672,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x7c01000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8,.advanced_ept_info=true};
 put(m,0x7c01000,0x7c02023);put(m,0x7c02018,0x7c06023);put(m,0x7c06ff0,0xffc000a3);put(m,0x7c06ff8,0x7c07023);put(m,0x7c07ff8,0xfffff063);
 uint8_t*rom=h->bytes+(m->flash[m->flash_pages-1]-BASE)+0xff0;rom[0]=0xc6;rom[1]=0x03;rom[2]=0x50;
 struct fvr_state probe={0};assert(fvr_init(&probe,m));uint64_t g[15]={0};g[1]=probe.address;
 struct fat_exit e={.rip=0xfffffff0,.qualification=0x58a,.linear=probe.address,.physical=probe.address,.rflags=0x10046,.dr7=0x400,.reason=48};
 struct fvd_plan p;struct fmm_memory invalid=f;invalid.memory=0;assert(!fvd_capture(&invalid,&e,g,&probe,&p)&&!fvd_capture(0,&e,g,&probe,&p));
 assert(fvd_capture(&f,&e,g,&probe,&p)&&p.length==3&&p.transaction.leaf_entry_gpa==0x7c06ff0&&p.transaction.leaf_value==0xffc000a3&&fvd_validate(&p,false)&&!fvd_validate(&p,true));
 struct fvd_plan corrupt=p;corrupt.transaction.leaf_entry_gpa+=8;assert(!fvd_validate(&corrupt,false));corrupt=p;corrupt.transaction.leaf_value^=0x10;assert(!fvd_validate(&corrupt,false));corrupt=p;corrupt.transaction.next_rip++;assert(!fvd_validate(&corrupt,false));corrupt=p;corrupt.transaction.rip++;assert(!fvd_validate(&corrupt,false));corrupt=p;corrupt.transaction.rflags^=1;assert(!fvd_validate(&corrupt,false));
 struct context c={.f=&f,.e=&e,.g=g,.s=&probe};struct fwd_ops ops={cap,val,cas,sample,writevm};struct fwd_state tx={0};
 assert(fwd_apply(&tx,true,true,&c,&ops)&&tx.complete&&tx.dirty_changed&&c.cas_count==1&&c.writes==2&&*c.plan.leaf_pointer==0xffc000e3&&fvd_validate(&c.plan,true));
 assert(!probe.stage&&h->bytes[m->flash[0]-BASE+16]==0x8d);unsigned calls=c.cas_count;assert(!fwd_apply(&tx,true,true,&c,&ops)&&c.cas_count==calls);
 /* All non-leaf snapshot changes stop before backend CAS, even after sample. */
 put(m,0x7c06ff0,0xffc000a3);c=(struct context){.f=&f,.e=&e,.g=g,.s=&probe,.mutate_ancestor=true};tx=(struct fwd_state){0};
 assert(!fwd_apply(&tx,true,true,&c,&ops)&&tx.failed&&!c.cas_count&&!c.writes);put(m,0x7c01000,0x7c02023);
 c=(struct context){.f=&f,.e=&e,.g=g,.s=&probe,.uncertain=true};tx=(struct fwd_state){0};
 assert(!fwd_apply(&tx,true,true,&c,&ops)&&tx.failed&&!tx.dirty_changed&&*c.plan.leaf_pointer==0xffc000e3&&!c.writes);calls=c.cas_count;assert(!fwd_apply(&tx,true,true,&c,&ops)&&c.cas_count==calls);
 put(m,0x7c06ff0,0xffc000a3);c=(struct context){.f=&f,.e=&e,.g=g,.s=&probe,.fail_write=true};tx=(struct fwd_state){0};assert(!fwd_apply(&tx,true,true,&c,&ops)&&tx.failed&&tx.dirty_changed&&*c.plan.leaf_pointer==0xffc000e3&&!probe.stage);
 /* ExistingD1 path has no backend mutation. */
 c=(struct context){.f=&f,.e=&e,.g=g,.s=&probe};tx=(struct fwd_state){0};assert(fwd_apply(&tx,true,true,&c,&ops)&&!tx.dirty_changed&&!c.cas_count);put(m,0x7c06ff0,0xffc000a3);
 assert(fvd_capture(&f,&e,g,&probe,&p));rom[2]=0x70;assert(!fvd_validate(&p,false));rom[2]=0x50;
 g[0]=1;assert(!fvd_validate(&p,false));g[0]=0;e.rflags=2;assert(!fvd_validate(&p,false));e.rflags=0x10046;f.cr0&=~0x10000ULL;assert(!fvd_validate(&p,false));f.cr0|=0x10000;
 put(m,0x7c07ff8,0xfffff073);assert(!fvd_validate(&p,false));put(m,0x7c07ff8,0xfffff063);
 struct sg_memory_ops saved_owner=m->owner;m->owner.context=0;assert(!fvd_validate(&p,false));m->owner=saved_owner;
 uint64_t leafs[]={0xffc00083,0xffc000a1,0xffc000a7,0xffc020a3,0xffc000a3ULL|(1ULL<<63),0xffc00023};
 for(unsigned i=0;i<sizeof leafs/sizeof*leafs;i++){put(m,0x7c06ff0,leafs[i]);assert(!fvd_capture(&f,&e,g,&probe,&p));}put(m,0x7c06ff0,0xffc000a3);
 /* Storage leaf's actual EPT must be normal private RWX WB37. */
 unsigned pt=4+(0x7c06000>>21),index=(0x7c06000>>12)&511;uint64_t*ep=(uint64_t*)(h->bytes+(m->tables[pt]-BASE));uint64_t saved=ep[index];ep[index]&=~2ULL;assert(!fvd_capture(&f,&e,g,&probe,&p));ep[index]=saved;
 saved=m->ram[1];m->ram[1]=m->ram[0x7c06];assert(!fvd_capture(&f,&e,g,&probe,&p));m->ram[1]=saved;
 saved=m->flash[1];m->flash[1]=m->ram[0x7c06];assert(!fvd_capture(&f,&e,g,&probe,&p));m->flash[1]=saved;
 saved=m->tables[4];m->tables[4]=m->ram[0x7c06];assert(!fvd_capture(&f,&e,g,&probe,&p));m->tables[4]=saved;
 /* Conservatively reject the same exact leaf entry in source and data walk,
  * even though CODE bytes would otherwise be a valid executable source. */
 uint8_t*codefirst=h->bytes+(m->flash[132]-BASE);codefirst[0]=0xc6;codefirst[1]=0x03;codefirst[2]=0x50;
 e.rip=0xffc84000;assert(!fvd_capture(&f,&e,g,&probe,&p));e.rip=0xfffffff0;codefirst[0]=codefirst[1]=codefirst[2]=0;
 /* Instruction backing alias to leaf storage must not be patchable. */
 uint64_t oldrip=e.rip;put(m,0x7c02000,0x7c08023);put(m,0x7c08000,0x7c09023);put(m,0x7c09030,0x7c06063);
 uint8_t instruction[]={0xc6,0x03,0x50};assert(fwm_write_ram(m,0x7c06000,instruction,3));e.rip=0x6000;assert(!fvd_capture(&f,&e,g,&probe,&p));e.rip=oldrip;
 assert(fvd_capture(&f,&e,g,&probe,&p));h->fail_map=h->maps+1;assert(!fvd_validate(&p,false));h->fail_map=0;
 assert(fwm_release(m)&&!h->live);free(code);free(vars);free(h->bytes);free(h);free(m);
 puts("VARS dirty adapter: real owned walks/EPT, source snapshot, post-sample conflict, uncertainD retained PASS");return 0;
}
