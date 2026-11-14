#include "create_context.h"
#include <assert.h>
#include <stdio.h>
struct desc {uint32_t type,pad;uint64_t base,virt,pages,attr;};
static struct desc map={7,0,0x100000,0,128,8};
int main(void){struct syle_boot_info b={0};b.magic=SYLE_BOOT_MAGIC;b.version=SYLE_BOOT_VERSION;b.size=sizeof(b);b.boot_services_exited=1;b.descriptor_size=sizeof(map);b.descriptor_version=1;b.memory_map=(uintptr_t)&map;b.memory_map_bytes=sizeof(map);b.map_buffer=(struct syle_range){(uintptr_t)&map,sizeof(map)};assert(pmm_init(&b));
 struct vtc c={0},next={0};struct vtc_entry entries[5]={0},entries2[5]={0};uint64_t ram,ept,vmxon,vmcs,pa;
 assert(vtc_open(&c,entries,5));assert(!vtc_allocate(&c,VLM_UNSPECIFIED,&pa));assert(vtc_allocate(&c,VLM_GUEST_RAM,&ram));assert(vtc_allocate(&c,VLM_EPT_TABLE,&ept));assert(vtc_allocate(&c,VLM_VMX_CONTROL,&vmxon));assert(vtc_allocate(&c,VLM_VMX_CONTROL,&vmcs));uint64_t before=pmm_get_stats().allocated_pages;assert(!vtc_allocate(&c,VLM_HOST_METADATA,&pa)&&pmm_get_stats().allocated_pages==before);
 assert(!vtc_pin_ram(&c,ept)&&!vtc_pin_ram(&c,vmxon)&&!vtc_pin_ram(&c,vmcs));entries[1].kind=VLM_GUEST_RAM;assert(!vtc_pin_ram(&c,ept));entries[1].kind=VLM_EPT_TABLE;
 assert(vtc_pin_ram(&c,ram)&&!vtc_release(&c,ram));assert(!vtc_close(&c)&&c.draining);assert(pmm_get_stats().allocated_pages==1);assert(!vtc_allocate(&c,VLM_GUEST_RAM,&pa));assert(vtc_unpin_ram(&c,ram)&&vtc_close(&c)&&!pmm_get_stats().allocated_pages);assert(!vtc_open(&c,entries,5));
 assert(vtc_open(&next,entries2,5)&&next.epoch!=c.epoch);assert(vtc_allocate(&next,VLM_GUEST_RAM,&pa)&&pa==ram);assert(!vtc_release(&c,pa)&&!vtc_pin_ram(&c,pa));assert(vtc_release(&next,pa));before=pmm_get_stats().allocated_pages;uint64_t sentinel=99;assert(!vtc_allocate(&next,VLM_GUEST_RAM,&sentinel)&&sentinel==99&&!next.failed);assert(pmm_get_stats().allocated_pages==before);assert(!vtc_release(&next,pa)&&!vtc_pin_ram(&next,pa));assert(vtc_close(&next)&&!pmm_get_stats().allocated_pages);
 struct vtc small={0},fresh={0};struct vtc_entry one[1]={0},two[1]={0};assert(vtc_open(&small,one,1));assert(vtc_allocate(&small,VLM_GUEST_RAM,&pa));uint64_t generation=one[0].allocation.generation;assert(vtc_release(&small,pa));before=pmm_get_stats().allocated_pages;assert(!vtc_allocate(&small,VLM_GUEST_RAM,&pa)&&pmm_get_stats().allocated_pages==before);assert(vtc_close(&small));assert(vtc_open(&fresh,two,1)&&vtc_allocate(&fresh,VLM_GUEST_RAM,&pa));assert(two[0].allocation.generation==generation+1);assert(vtc_close(&fresh));
 puts("per-CREATE typed context: PASS (tokens/pins only, no mapping/DMA)");return 0;}
