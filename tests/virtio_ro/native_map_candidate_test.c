#define _GNU_SOURCE
#define PAGING_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#define pmm_alloc_page_below test_table_allocate
#define pmm_address_limit test_physical_limit
#include "../../krnl/paging.c"
#undef pmm_alloc_page_below
#undef pmm_address_limit
#include "native_map_candidate.h"
#include "../../krnl/vmx_memory.h"
char __kernel_start[1],__text_end[1],__rodata_start[1],__rodata_end[1],__kernel_end[1];
static uint64_t cr3,cr4,pat=6;
static bool context_ok=true;
static struct vmx_memory_snapshot mt;
static unsigned char external[4096];
int paging_test_context(void){return 1;}void paging_test_flush(void){}
uint64_t paging_test_cr3(void){return cr3;}uint64_t paging_test_cr4(void){return cr4;}uint64_t paging_test_pat(void){return pat;}
uint64_t test_physical_limit(void){return UINT64_C(1)<<47;}
uint64_t test_table_allocate(uint64_t limit){void *p=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);assert(p!=MAP_FAILED&&(uintptr_t)p<limit);return (uintptr_t)p;}
void panic(const char *msg){fprintf(stderr,"panic %s\n",msg);abort();}void log_text(const char *s){(void)s;}void log_hex(UINT64 n){(void)n;}
bool vnm_test_context(void){return context_ok;}
int vmx_memory_region_wb(uint64_t pa){return vmx_memory_snapshot_region_wb(&mt,pa);}
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,0,0,2,8};
int main(int argc,char **argv){assert(argc==1||argc==2);
 void *backing=mmap(0,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);assert(backing!=MAP_FAILED);desc.base=(uintptr_t)backing;
 struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof(bi);bi.boot_services_exited=1;bi.descriptor_size=sizeof(desc);bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof(desc);bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof(desc)};assert(pmm_init(&bi));
 root=new_table();cr3=root;*leaf(desc.base,1)=desc.base|3;*leaf(desc.base+4096,1)=(desc.base+4096)|3;
 mt=(struct vmx_memory_snapshot){.cpu_mtrr=1,.cpu_pat=1,.physical_bits=36,.pat=6,.def_type=0x806};
 struct vtc_entry e[2]={0};struct vsa a={.storage=e,.capacity=2};assert(vsa_begin(&a));struct vsc s={0};struct vnm map={0};struct vsc_ops o;
 assert(vnm_bind(&map,&s,external,sizeof external,&o)&&vsc_init(&s,&a,&o));struct vlm_allocation token;assert(vsc_allocate(&s,SG_MEMORY_RAM,&token));
 memset(external,0x6b,sizeof external);uint64_t p;assert(pna_page(a.owned.epoch,token,&p)&&p==desc.base);
 if(argc==2){
 if(!strcmp(argv[1],"pat"))pat=0;
 else if(!strcmp(argv[1],"mtrr"))mt.def_type=0x800;
 else if(!strcmp(argv[1],"wrongpa"))*leaf(p,0)=(p+4096)|3;
 else if(!strcmp(argv[1],"absent"))*leaf(p,0)=0;
 else if(!strcmp(argv[1],"la57"))cr4=UINT64_C(1)<<12;
 else if(!strcmp(argv[1],"user")){for(int shift=39;shift>=12;shift-=9)*entry(p,0,shift)|=4;}
 else *leaf(p,0)=p|1;
 assert(!vsc_copy(&s,token,VLM_GUEST_RAM,0,external,1,true)&&s.faulted&&*(unsigned char*)backing==0);}
 else{context_ok=false;assert(!vsc_copy(&s,token,VLM_GUEST_RAM,0,external,1,true)&&!s.faulted);context_ok=true;
  cr3++;assert(!vsc_copy(&s,token,VLM_GUEST_RAM,0,external,1,true)&&s.faulted);cr3--;/* New context required after transfer rejection. */
 }
 assert(vsc_drain(&s)&&vsc_retire_after_quiescence(&s)&&!pmm_get_stats().allocated_pages);
 /* Fresh stable objects for success path. */
 struct vtc_entry e2[2]={0};struct vsa a2={.storage=e2,.capacity=2};struct vsc s2={0};struct vnm map2={0};assert(vsa_begin(&a2));pat=6;cr4=0;mt.def_type=0x806;for(int shift=39;shift>=12;shift-=9)*entry(p,0,shift)&=~UINT64_C(4);*leaf(p,0)=p|3;
 assert(vnm_bind(&map2,&s2,external,sizeof external,&o)&&vsc_init(&s2,&a2,&o)&&vsc_allocate(&s2,SG_MEMORY_RAM,&token));
 assert(vsc_copy(&s2,token,VLM_GUEST_RAM,4095,external,1,true)&&((unsigned char*)backing)[4095]==0x6b);
 external[0]=0;assert(vsc_copy(&s2,token,VLM_GUEST_RAM,4095,external,1,false)&&external[0]==0x6b);
 assert(vsc_drain(&s2)&&vsc_retire_after_quiescence(&s2)&&!pmm_get_stats().allocated_pages);
 puts("actual paging walk/phys translation + MTRR policy + PMM/scoped copy: host permission/CR3 gates PASS; native NMI entry remains fail-stop");return 0;
}
