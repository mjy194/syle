#include "session_authority.h"
#include <assert.h>
#include <stdio.h>
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,0x100000,0,4,8};
int main(void){
 struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof(bi);bi.boot_services_exited=1;bi.descriptor_size=sizeof(desc);bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof(desc);bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof(desc)};assert(pmm_init(&bi));
 struct vtc_entry e[4]={0};struct vsa a={.storage=e,.capacity=4};enum vlm_kind k;
 assert(vsa_begin(&a)&&!vsa_begin(&a));uint64_t ram=vsa_allocate(&a,SG_MEMORY_RAM),ept=vsa_allocate(&a,SG_MEMORY_EPT),on=vsa_allocate(&a,SG_MEMORY_VMXON),cs=vsa_allocate(&a,SG_MEMORY_VMCS);
 assert(ram&&ept&&on&&cs&&ram!=ept&&on!=cs);
 assert(vsa_query(&a,ram,&k)&&k==VLM_GUEST_RAM);assert(vsa_query(&a,ept,&k)&&k==VLM_EPT_TABLE);
 assert(vsa_query(&a,on,&k)&&k==VLM_VMX_CONTROL&&vsa_query(&a,cs,&k)&&k==VLM_VMX_CONTROL);
 assert(!vsa_allocate(&a,SG_MEMORY_RAM)&&!pmm_free_page(ram));
 e[0].kind=VLM_EPT_TABLE;assert(!vsa_query(&a,ram,&k)&&!vsa_release(&a,ram));e[0].kind=VLM_GUEST_RAM;
 /* Quarantine alias/unknown lookup cannot bypass published-page cleanup. */
 struct vlm_allocation saved=e[1].allocation;e[1].quarantined=true;e[1].allocation=e[0].allocation;
 assert(!vsa_release(&a,ram)&&pmm_get_stats().allocated_pages==4);
 e[1].allocation.generation=0;
 assert(!vsa_release(&a,ram)&&pmm_get_stats().allocated_pages==4);
 e[1].allocation=saved;e[1].quarantined=false;
 assert(vtc_pin_ram(&a.owned,ram));assert(!vsa_release(&a,ram));
 /* A later admission failure must not deadlock cleanup of published pages. */
 a.owned.failed=true;
 assert(vsa_release(&a,ept)&&vsa_release(&a,on)&&vsa_release(&a,cs));
 assert(!vsa_end(&a)&&pmm_get_stats().allocated_pages==1);
 assert(!vsa_allocate(&a,SG_MEMORY_RAM));assert(vtc_unpin_ram(&a.owned,ram));assert(vsa_end(&a)&&!pmm_get_stats().allocated_pages&&vsa_end(&a));
 struct vtc_entry e2[4]={0};struct vsa b={.storage=e2,.capacity=4};assert(vsa_begin(&b));uint64_t reused=vsa_allocate(&b,SG_MEMORY_RAM);assert(reused==ram);
 assert(!vsa_query(&a,reused,&k)&&!vsa_release(&a,reused)&&!vsa_begin(&a));assert(vsa_query(&b,reused,&k)&&vsa_release(&b,reused)&&vsa_end(&b));
 puts("session authority: actual PMM epoch/type/generation, pin/end retention, stale-context rejection PASS; no mappings/DMA");
}
