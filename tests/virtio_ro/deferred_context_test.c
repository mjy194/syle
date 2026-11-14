#include "deferred_context.h"
#include <assert.h>
#include <stdio.h>
struct desc {uint32_t type,pad;uint64_t base,virt,pages,attr;};
static struct desc map={7,0,0x100000,0,128,8};
int main(void){struct syle_boot_info b={0};b.magic=SYLE_BOOT_MAGIC;b.version=SYLE_BOOT_VERSION;b.size=sizeof(b);b.boot_services_exited=1;b.descriptor_size=sizeof(map);b.descriptor_version=1;b.memory_map=(uintptr_t)&map;b.memory_map_bytes=sizeof(map);b.map_buffer=(struct syle_range){(uintptr_t)&map,sizeof(map)};assert(pmm_init(&b));struct vdc c={0},next={0};struct vdc_entry entries[4]={0},newentries[4]={0};uint64_t pages[4],pa;
 assert(!vdc_open(&c,entries,UINT32_MAX)&&!c.opened);assert(!vdc_open(&c,entries,5)&&!c.opened);assert(pna_capacity()==4);assert(vdc_open(&c,entries,4));
 for(unsigned i=0;i<4;i++){assert(vdc_allocate(&c,VLM_GUEST_RAM,&pages[i]));for(unsigned j=0;j<i;j++)assert(pages[i]!=pages[j]);assert(vdc_pin_ram(&c,pages[i]));assert(!vdc_retire(&c,pages[i]));assert(vdc_unpin(&c,pages[i]));assert(vdc_retire(&c,pages[i]));assert(!vdc_pin_ram(&c,pages[i])&&!vdc_retire(&c,pages[i]));assert(!pmm_free_page(pages[i]));assert(pmm_get_stats().allocated_pages==i+1);}
 pa=99;assert(!vdc_allocate(&c,VLM_GUEST_RAM,&pa)&&pa==99&&pmm_get_stats().allocated_pages==4);assert(vdc_finalize(&c)&&!pmm_get_stats().allocated_pages);
 assert(vdc_open(&next,newentries,4));assert(vdc_allocate(&next,VLM_GUEST_RAM,&pa)&&pa==pages[0]);assert(!vdc_pin_ram(&c,pa)&&!vdc_retire(&c,pa));assert(vdc_pin_ram(&next,pa));uint64_t ept;assert(vdc_allocate(&next,VLM_EPT_TABLE,&ept));assert(pmm_get_stats().allocated_pages==2);assert(!vdc_finalize(&next)&&pmm_get_stats().allocated_pages==1&&newentries[0].present&&!newentries[1].present);assert(!vdc_finalize(&next)&&pmm_get_stats().allocated_pages==1);uint64_t sentinel=99;assert(!vdc_allocate(&next,VLM_GUEST_RAM,&sentinel)&&sentinel==99);assert(vdc_unpin(&next,pa)&&vdc_finalize(&next)&&!pmm_get_stats().allocated_pages);
 puts("deferred physical reclamation: bounded churn PASS (host-only; no sg callback)");return 0;}
