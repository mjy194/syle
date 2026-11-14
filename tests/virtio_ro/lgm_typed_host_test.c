/* Host-only integration of REAL lgm_prepare/release and actual optional PMM.
 * map returns a malloc-backed host fixture, never paging_phys_to_virt.
 * Roles come from explicit real LGM allocation sites; no ordinal role inference. */
#define _POSIX_C_SOURCE 200112L
#include "native_candidate.h"
#include "../../krnl/linux_guest_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COUNT (LGM_PAGE_COUNT+LGM_TABLE_COUNT)
#define TOTAL (COUNT+2)
#define BASE UINT64_C(0x100000)
struct record {uint64_t pa;struct vlm_allocation a;enum vlm_kind kind;bool live;};
struct bridge {uint64_t epoch;unsigned allocated,fail_alloc,fail_map,fail_wb;uint64_t fail_release;uint8_t *bytes;struct record records[TOTAL];};
struct desc {uint32_t type,pad;uint64_t base,virt,pages,attr;};
static struct desc map_desc={7,0,BASE,0,TOTAL,8};
static unsigned idx(uint64_t pa){assert(pa>=BASE&&pa<BASE+(uint64_t)TOTAL*4096&&!(pa&4095));return (unsigned)((pa-BASE)/4096);}
static struct record *record(struct bridge *b,uint64_t pa){unsigned i=idx(pa);struct record *r=&b->records[i];uint64_t actual;enum vlm_kind kind;if(!r->live||r->pa!=pa||!pna_page(b->epoch,r->a,&actual)||actual!=pa||!pna_kind(b->epoch,r->a,&kind)||kind!=r->kind)return 0;return r;}
static uint64_t alloc_kind(struct bridge *b,enum vlm_kind kind){
 unsigned n=b->allocated;if(n>=TOTAL||n+1==b->fail_alloc)return 0;
 struct record *r=&b->records[n];if(!pna_alloc_typed(b->epoch,kind,&r->a))return 0;r->live=true;r->kind=kind;assert(pna_page(b->epoch,r->a,&r->pa));assert(r->pa==BASE+(uint64_t)n*4096);b->allocated++;return r->pa;
}
static uint64_t alloc_lgm_role(void *v,enum sg_memory_role role){struct bridge *b=v;if(b->allocated>=COUNT)return 0;switch(role){case SG_MEMORY_RAM:assert(b->allocated<LGM_PAGE_COUNT);return alloc_kind(b,VLM_GUEST_RAM);case SG_MEMORY_EPT:assert(b->allocated>=LGM_PAGE_COUNT);return alloc_kind(b,VLM_EPT_TABLE);default:assert(0);return 0;}}
static bool release(void *v,uint64_t pa){struct bridge *b=v;struct record *r=record(b,pa);if(!r||pa==b->fail_release)return false;if(!pna_release(b->epoch,r->a))return false;r->live=false;return true;}
static void *mapping(void *v,uint64_t pa){struct bridge *b=v;unsigned i=idx(pa);if(i+1==b->fail_map||!record(b,pa))return 0;return b->bytes+(size_t)i*4096;}
static bool wb(void *v,uint64_t pa){struct bridge *b=v;return idx(pa)+1!=b->fail_wb&&record(b,pa)!=0;}
static bool seal(struct bridge *b,const struct linux_guest_memory *m){
 if(!m->prepared||b->allocated!=COUNT||m->owner.context!=b)return false;
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++){struct record *r=record(b,m->pages[i]);if(!r||r!=&b->records[i]||r->kind!=VLM_GUEST_RAM)return false;}
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++){struct record *r=record(b,m->tables[i]);if(!r||r!=&b->records[LGM_PAGE_COUNT+i]||r->kind!=VLM_EPT_TABLE)return false;}
 return true;
}
static void setup_pmm(void){struct syle_boot_info b={0};b.magic=SYLE_BOOT_MAGIC;b.version=SYLE_BOOT_VERSION;b.size=sizeof(b);b.boot_services_exited=1;b.descriptor_size=sizeof(map_desc);b.descriptor_version=1;b.memory_map=(uintptr_t)&map_desc;b.memory_map_bytes=sizeof(map_desc);b.map_buffer=(struct syle_range){(uintptr_t)&map_desc,sizeof(map_desc)};assert(pmm_init(&b));}
static void *file(const char *path,size_t *n){FILE *f=fopen(path,"rb");assert(f&&fseek(f,0,SEEK_END)==0);long z=ftell(f);assert(z>0);*n=(size_t)z;rewind(f);void *p=malloc(*n);assert(p&&fread(p,1,*n,f)==*n);fclose(f);return p;}
int main(int argc,char **argv){assert(argc==3||argc==4);setup_pmm();size_t kn,rn;void *kernel=file(argv[1],&kn),*initrd=file(argv[2],&rn);struct bridge *b=calloc(1,sizeof(*b));struct linux_guest_memory *m=calloc(1,sizeof(*m));assert(b&&m);assert(!posix_memalign((void**)&b->bytes,4096,(size_t)TOTAL*4096));assert(pna_start(&b->epoch));struct sg_memory_ops ops={b,0,release,mapping,wb};assert(!lgm_prepare_roles(m,&ops,0,kernel,kn,initrd,rn,"console=ttyS0 rdinit=/init"));
 if(argc==4){if(!strcmp(argv[3],"alloc"))b->fail_alloc=LGM_PAGE_COUNT+1;else if(!strcmp(argv[3],"wb"))b->fail_wb=LGM_PAGE_COUNT+1;else b->fail_map=LGM_PAGE_COUNT+1;
  assert(!lgm_prepare_roles(m,&ops,alloc_lgm_role,kernel,kn,initrd,rn,"console=ttyS0 rdinit=/init"));assert(!m->prepared&&!pmm_get_stats().allocated_pages);assert(pna_drain(b->epoch)&&pna_end(b->epoch));puts("real LGM typed callback role-boundary failure cleanup: PASS (host maps only)");
 }else{
  assert(lgm_prepare_roles(m,&ops,alloc_lgm_role,kernel,kn,initrd,rn,"console=ttyS0 rdinit=/init")&&seal(b,m));assert(pmm_get_stats().allocated_pages==COUNT);b->records[0].kind=VLM_EPT_TABLE;assert(!mapping(b,m->pages[0])&&!release(b,m->pages[0]));b->records[0].kind=VLM_GUEST_RAM;assert(mapping(b,m->pages[0]));assert(!memcmp(b->bytes+m->plan.initrd_addr,initrd,rn));uint64_t keep=m->tables[0];m->tables[0]=m->pages[0];assert(!seal(b,m));m->tables[0]=keep;
  uint64_t controls[2];for(unsigned i=0;i<2;i++){controls[i]=alloc_kind(b,VLM_VMX_CONTROL);assert(controls[i]);struct record *r=record(b,controls[i]);assert(r&&r->kind==VLM_VMX_CONTROL);}
  assert(!pmm_alloc_page());assert(pmm_get_stats().allocated_pages==TOTAL);b->fail_release=m->pages[0];assert(!lgm_release(m)&&m->pages[0]==b->fail_release);assert(pmm_get_stats().allocated_pages==3);assert(!pna_end(b->epoch));assert(pna_drain(b->epoch));assert(!pna_end(b->epoch));b->fail_release=0;assert(lgm_release(m));for(unsigned i=0;i<2;i++)assert(release(b,controls[i]));assert(!pmm_get_stats().allocated_pages&&pna_end(b->epoch));uint64_t newer;struct vlm_allocation replacement;assert(pna_start(&newer)&&newer!=b->epoch&&pna_alloc_typed(newer,VLM_GUEST_RAM,&replacement));uint64_t reused;assert(pna_page(newer,replacement,&reused)&&reused==BASE);b->records[0].live=true;assert(!mapping(b,reused)&&!release(b,reused));assert(pna_page(newer,replacement,&reused)&&reused==BASE);b->records[0].live=false;assert(pna_release(newer,replacement)&&pna_drain(newer)&&pna_end(newer));puts("real LGM + actual PMM typed callback/retained release: PASS (no VMX or DMA)");
 }
 free(b->bytes);free(b);free(m);free(kernel);free(initrd);return 0;}
