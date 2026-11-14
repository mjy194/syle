#define _POSIX_C_SOURCE 200112L
#define main legacy_host_main
#include "../../krnl/tests/linux_vcpu_host.c"
#undef main
#include "../../krnl/linux_guest_role_binding.h"
#include "session_authority.h"
#include <setjmp.h>
#include <stdlib.h>
#define COUNT (LGM_PAGE_COUNT+LGM_TABLE_COUNT)
#define TOTAL (COUNT+2)
#define BASE UINT64_C(0x100000)
static struct lgs_session session;
static struct lgr_binding binding;
static struct guest_control ctl;
static struct vsa authority;
static struct vtc_entry entries[TOTAL];
static struct vlm_handle pins[TOTAL];
static bool held[TOTAL],deny_barrier,partial,deny_end;
static unsigned char *bytes;
static uint64_t fail_map;
static unsigned barriers,disables,ends;
static uint64_t epoch,retain;
static bool disable_fail,expect_fatal;
static jmp_buf fatal_jump;
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,BASE,0,TOTAL,8};
static bool begin(void *ctx){assert(ctx==&authority);assert(guest_control_call(&ctl,1,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBUSY);if(!vsa_begin(ctx))return false;epoch=authority.owned.epoch;return true;}
static bool end(void *ctx){ends++;assert(!session.enabled&&!m.live&&!session.memory.prepared);return !deny_end&&vsa_end(ctx);}
static bool lookup(uint64_t pa,unsigned *out){
 if(pa<BASE||(pa&4095)||(pa-BASE)/4096>=TOTAL)return false;
 unsigned n=(unsigned)((pa-BASE)/4096);struct vtc_entry *e=&entries[n];uint64_t actual;enum vlm_kind kind;
 if(!e->live||e->pa!=pa||e->quarantined||!pna_page(epoch,e->allocation,&actual)||actual!=pa||!pna_kind(epoch,e->allocation,&kind)||kind!=e->kind)return false;
 *out=n;return true;
}
static uint64_t allocate_role(void *ctx,enum sg_memory_role role){
 uint64_t pa=vsa_allocate(ctx,role);if(!pa)return 0;unsigned n;assert(lookup(pa,&n));
 assert(pna_pin(epoch,entries[n].allocation,&pins[n]));held[n]=true;
 if(role==SG_MEMORY_VMXON||role==SG_MEMORY_VMCS){assert(n>=COUNT);m.used[n-COUNT]=true;m.live++;m.alloc++;}
 return pa;
}
static bool free_owned(void *ctx,uint64_t pa){
 unsigned n;if(!lookup(pa,&n)||pa==retain)return false;
 if(n>=COUNT)assert(!m.active&&m.used[n-COUNT]);
 else assert(!session.enabled&&!session.host.active);
 if(!vsa_release(ctx,pa))return false;
 if(n>=COUNT){m.used[n-COUNT]=false;m.live--;}
 return true;
}
static void *map_owned(void *ctx,uint64_t pa){unsigned n;if(ctx!=&authority||pa==fail_map||!lookup(pa,&n)||!held[n])return 0;return bytes+(size_t)n*4096;}
static bool wb_owned(void *ctx,uint64_t pa){unsigned n;return ctx==&authority&&lookup(pa,&n)&&held[n];}
static bool vmx_on(void *ctx,uint64_t pa){(void)ctx;unsigned n;assert(!m.active&&lookup(pa,&n)&&n==COUNT&&held[n]);if(!tick())return false;m.active=true;return true;}
static bool vmx_clear(void *ctx,uint64_t pa){(void)ctx;unsigned n;assert(m.active&&lookup(pa,&n)&&n==COUNT+1&&held[n]);return tick()&&m.block!=1;}
static bool vmx_load(void *ctx,uint64_t pa){(void)ctx;unsigned n;assert(m.active&&lookup(pa,&n)&&n==COUNT+1&&held[n]);return tick();}
static bool prepared(void *ctx,struct lgs_session *s){(void)ctx;assert(s==&session);ram.eptp=s->memory.eptp;return true;}
static bool barrier(void *ctx){assert(ctx==&authority&&binding.close_inflight&&!session.host.active&&!session.host.ready&&!m.active);
 assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)==-SYLE_GUEST_EBUSY);
 assert(guest_control_call(&ctl,1,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBUSY);
 barriers++;if(deny_barrier||epoch!=authority.owned.epoch)return false;
 for(unsigned n=0;n<TOTAL;n++)if(held[n]){if(!pna_unpin(epoch,pins[n]))return false;held[n]=false;if(partial){partial=false;return false;}}
 return true;
}
static bool enable(void *ctx){(void)ctx;return true;}
static bool disable(void *ctx){(void)ctx;assert(!session.host.active&&!m.active&&!m.live);for(unsigned n=0;n<TOTAL;n++)assert(!held[n]);disables++;return !disable_fail;}
static bool safe(void *ctx){(void)ctx;return true;}
static bool read_field(void *ctx,uint32_t field,uint64_t *v){(void)ctx;(void)field;(void)v;return false;}
static bool dispatch(void *ctx,struct lgs_session*s,uint32_t reason){(void)ctx;(void)s;(void)reason;return false;}
static void fatal(void *ctx){(void)ctx;if(expect_fatal)longjmp(fatal_jump,1);assert(!"unexpected dirty host");}
bool lvs_init(struct lvs_state *s){s->ready=true;return true;}
enum lvs_result lvs_run(struct lvs_state*s,const struct lvs_ops*o){(void)s;(void)o;assert(!"no guest execution");return LVS_ENTRY_FAILED;}
static void *load_asset(const char *path,size_t *n){FILE *fp=fopen(path,"rb");assert(fp&&!fseek(fp,0,SEEK_END));long z=ftell(fp);assert(z>0);*n=(size_t)z;rewind(fp);void *p=malloc(*n);assert(p&&fread(p,1,*n,fp)==*n);fclose(fp);return p;}
int main(int argc,char **argv){
 assert(argc==3||argc==4);assert(!legacy_host_main());reset();
 struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof(bi);bi.boot_services_exited=1;bi.descriptor_size=sizeof(desc);bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof(desc);bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof(desc)};assert(pmm_init(&bi));
 authority.storage=entries;authority.capacity=TOTAL;assert(!posix_memalign((void**)&bytes,4096,(size_t)TOTAL*4096));size_t kn,rn;void *kernel=load_asset(argv[1],&kn),*initrd=load_asset(argv[2],&rn);
 ops.memory=(struct sg_memory_ops){&authority,0,free_owned,map_owned,wb_owned};ops.vmxon=vmx_on;ops.vmclear=vmx_clear;ops.vmptrld=vmx_load;
 struct lvs_ops v={0};struct lgs_platform p={.enable=enable,.disable=disable,.safe=safe,.read_field=read_field,.dispatch=dispatch,.fatal=fatal,.prepare_guest=prepared};
 assert(lgs_configure(&session,&ops,&v,&p,kernel,kn,initrd,rn,"console=ttyS0 rdinit=/init",0x80000,0x400000));
 assert(lgr_configure_barrier(&binding,&session,&authority,allocate_role,begin,end,barrier,&authority));guest_control_init(&ctl,1,&lgr_guest_backend,&binding);
 if(argc==4){
  fail_map=BASE+(uint64_t)LGM_PAGE_COUNT*4096;
  assert(guest_control_call(&ctl,1,SYLE_GUEST_CREATE,0,1)==-SYLE_GUEST_EBACKEND);
  assert(ctl.state==SYLE_GUEST_EMPTY&&!session.enabled&&!m.active&&!pmm_get_stats().allocated_pages&&ends==1&&barriers==1);
  puts("actual controller/session/LGM/lvh/PMM: first EPT map failure + pinned rollback through barrier PASS; host maps/mock VMX");
 }else{
  assert(guest_control_call(&ctl,1,SYLE_GUEST_CREATE,0,1)==1&&pmm_get_stats().allocated_pages==TOTAL&&session.host.active);
  assert(!memcmp(bytes+session.memory.plan.initrd_addr,initrd,rn));
  m.block=3;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&m.active&&!barriers&&!disables&&pmm_get_stats().allocated_pages==TOTAL);
  m.block=0;deny_barrier=true;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!m.active&&barriers==1&&!disables&&pmm_get_stats().allocated_pages==TOTAL);unsigned calls=m.calls;
  deny_barrier=false;uint64_t saved=epoch;epoch++;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!disables);epoch=saved;
  partial=true;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!held[0]&&held[1]&&m.calls==calls);
  retain=session.memory.pages[0];assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!m.live&&disables==1&&!ends&&pmm_get_stats().allocated_pages==1&&session.memory.pages[0]==retain&&m.calls==calls);
  retain=0;deny_end=true;assert(guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)<0&&!pmm_get_stats().allocated_pages&&binding.base_released&&ends==1);
  deny_end=false;assert(!guest_control_call(&ctl,1,SYLE_GUEST_REAP,0,0)&&ends==2&&disables==1&&ctl.state==SYLE_GUEST_EMPTY&&m.calls==calls);
  puts("actual controller/session/128MiB LGM/lvh/PMM pins: off/barrier/partial-unpin/RAM-free/end REAP PASS; host maps/mock VMX, no DMA");
 }
 free(bytes);free(kernel);free(initrd);return 0;
}
