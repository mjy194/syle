#define _POSIX_C_SOURCE 200112L
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main fixture_main
#define lgm_prepare fixture_lgm_prepare
#define lgm_release fixture_lgm_release
#define lvh_close fixture_lvh_close
#include "../../krnl/tests/linux_guest_session.c"
#undef main
#undef lgm_prepare
#undef lgm_release
#undef lvh_close
#pragma GCC diagnostic pop
#include "../../krnl/linux_guest_role_binding.h"
#include "session_authority.h"
#include <stdlib.h>
#define TOTAL (LGM_PAGE_COUNT+LGM_TABLE_COUNT+2)
#define BASE UINT64_C(0x100000)
static struct vsa a;
static struct vtc_entry inventory[TOTAL];
static unsigned char *bytes;
static uint64_t on,cs,fail_pa;
static bool end_fail;
static unsigned ends;
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,BASE,0,TOTAL,8};
static void *mapping(void *ctx,uint64_t pa){
 assert(ctx==&a);
 if(pa<BASE||(pa&4095)||(pa-BASE)/4096>=TOTAL||pa==fail_pa)return 0;
 unsigned n=(unsigned)((pa-BASE)/4096);struct vtc_entry *e=&inventory[n];uint64_t actual;enum vlm_kind kind;
 /* Contiguous PA/index relation is this host fixture only, NOT native lookup. */
 if(!e->live||e->pa!=pa||e->quarantined||!pna_page(a.owned.epoch,e->allocation,&actual)||actual!=pa||!pna_kind(a.owned.epoch,e->allocation,&kind)||kind!=e->kind)return 0;
 return bytes+(size_t)n*4096;
}
static bool page_wb(void *ctx,uint64_t pa){
 /* WB fixture verifies ownership independently so fail_pa tests map failure,
  * not an earlier writeback rejection. No native memory-type claim. */
 enum vlm_kind kind;return ctx==&a&&vsa_query(&a,pa,&kind);
}
static bool inv(void *ctx,uint64_t eptp){(void)ctx;return eptp==s.memory.eptp;}
bool lvh_open_roles(struct lvh_context*h,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip,sg_allocate_role_fn role){
 on=role(o->memory.context,SG_MEMORY_VMXON);cs=role(o->memory.context,SG_MEMORY_VMCS);assert(on&&cs);return lvh_open(h,o,m,rsp,rip);
}
bool lvh_close(struct lvh_context*h){if(!fixture_lvh_close(h))return false;if(cs){assert(vsa_release(&a,cs));cs=0;}if(on){assert(vsa_release(&a,on));on=0;}return true;}
static bool end_epoch(void *ctx){ends++;assert(!on&&!cs&&!s.enabled&&!s.host.active&&!s.memory.prepared);return !end_fail&&vsa_end(ctx);}
static void *load(const char *path,size_t *n){FILE *fp=fopen(path,"rb");assert(fp&&!fseek(fp,0,SEEK_END));long z=ftell(fp);assert(z>0);*n=(size_t)z;rewind(fp);void *p=malloc(*n);assert(p&&fread(p,1,*n,fp)==*n);fclose(fp);return p;}
/* This fixture exercises the no-barrier configuration only. */
bool lvh_close_with_barrier(struct lvh_context *h,bool (*hook)(void *),void *ctx){
 (void)h;(void)hook;(void)ctx;assert(!"barrier requires actual lvh test");return false;
}
int main(int argc,char **argv){
 assert(argc==3||argc==4);struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof(bi);bi.boot_services_exited=1;bi.descriptor_size=sizeof(desc);bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof(desc);bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof(desc)};assert(pmm_init(&bi));
 a.storage=inventory;a.capacity=TOTAL;assert(!posix_memalign((void**)&bytes,4096,(size_t)TOTAL*4096));size_t kn,rn;void *kernel=load(argv[1],&kn),*initrd=load(argv[2],&rn);
 struct lvh_ops h={.memory={.context=&a,.release=vsa_release,.map=mapping,.writeback=page_wb},.vmwrite=write,.invept=inv};struct lvs_ops v={0};struct lgs_platform p={.enable=enable,.disable=disable,.safe=safe,.read_field=rd,.dispatch=dispatch,.fatal=fatal};struct lgr_binding b={0};
 assert(lgs_configure(&s,&h,&v,&p,kernel,kn,initrd,rn,"console=ttyS0 rdinit=/init",0x1000,0x1000));assert(lgr_configure(&b,&s,&a,vsa_allocate,vsa_begin,end_epoch));
 if(argc==4){fail_pa=BASE+(uint64_t)LGM_PAGE_COUNT*4096;assert(lgr_guest_backend.prepare(&b,1));assert(!s.enabled&&!s.host.active&&!pmm_get_stats().allocated_pages&&!ends);assert(lgr_guest_backend.prepare(&b,1));assert(!lgr_guest_backend.release(&b)&&ends==1&&a.owned.ended);puts("actual session/LGM/PMM first-EPT map failure rollback PASS; no native mappings");}
 else{
 assert(!lgr_guest_backend.prepare(&b,1)&&pmm_get_stats().allocated_pages==TOTAL);
 uint64_t ram=s.memory.pages[0];enum vlm_kind kind;assert(vsa_query(&a,ram,&kind)&&kind==VLM_GUEST_RAM);assert(vsa_query(&a,s.memory.tables[0],&kind)&&kind==VLM_EPT_TABLE);
 assert(!memcmp(bytes+s.memory.plan.initrd_addr,initrd,rn));inventory[0].kind=VLM_EPT_TABLE;assert(!mapping(&a,ram));inventory[0].kind=VLM_GUEST_RAM;
 assert(!lgr_guest_backend.stop(&b)&&pmm_get_stats().allocated_pages==TOTAL&&!ends);
 f.close_fail=1;assert(lgr_guest_backend.release(&b)&&pmm_get_stats().allocated_pages==TOTAL&&!ends);
 f.close_fail=0;assert(vtc_pin_ram(&a.owned,ram));assert(lgr_guest_backend.release(&b)&&pmm_get_stats().allocated_pages==1&&!ends&&s.memory.pages[0]==ram);
 assert(lgr_guest_backend.prepare(&b,1)&&lgr_guest_backend.start(&b));assert(vtc_unpin_ram(&a.owned,ram));
 end_fail=true;assert(lgr_guest_backend.release(&b)&&!pmm_get_stats().allocated_pages&&ends==1&&b.base_released);
 unsigned closes=f.closes;end_fail=false;assert(!lgr_guest_backend.release(&b)&&a.owned.ended&&ends==2&&f.closes==closes);
 assert(!mapping(&a,ram));puts("actual session + actual 128MiB LGM + actual PMM epoch, mocked VMX: ownership/close/pin/end retention PASS; no DMA");
 }
 free(bytes);free(kernel);free(initrd);return 0;
}
