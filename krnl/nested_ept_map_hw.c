#include "nested_ept_map_hw.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
static uint64_t *ptr(uint64_t p){return paging_phys_to_virt(p);}
static bool resolve(void *ctx,uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *perm) {
    (void)need;struct ept_fixture *f=ctx;
    if((gpa&4095)||gpa<4096||gpa>=((uint64_t)EPT_GUEST_PAGES+1)*4096)return false;
    for(unsigned i=0;i<3;i++)if(!ptr(f->tables[i])||ptr(f->tables[i])[0]!=(f->tables[i+1]|7))return false;
    if(!ptr(f->tables[3]))return false;
    unsigned page=(unsigned)(gpa>>12);
    uint64_t leaf=ptr(f->tables[3])[page];
    if((leaf&~UINT64_C(7))!=(f->pages[page-1]|(6u<<3))||!(leaf&7))return false;
    *hpa=f->pages[page-1];*view=ptr(*hpa);*perm=leaf&7;
    return *view!=0;
}
bool nested_ept_map_hw_flush(struct nested_ept_map_hw *c) {
#ifdef NESTED_MAP_HW_UNIT_TEST
    extern bool nested_map_test_flush(uint64_t);
    return nested_map_test_flush(c->eptp);
#else
    struct {uint64_t p,reserved;} d __attribute__((aligned(16)))={c->eptp,0};
    uint64_t type=1;unsigned char fail;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(d),"r"(type):"cc","memory");return !fail;
#endif
}
bool nested_ept_map_hw_rebuild(struct nested_ept_map_hw *c,struct ept_fixture *f) {
    if(c->poisoned||c->generation==UINT64_MAX)return false;
    struct nested_ept_map next;
    if(!nested_ept_map_build(&next,0x1001e,resolve,f))return false;
    /* Validation finished before any mutation. Single vCPU is stopped. Clear
     * every bounded leaf, including unmaps. Remaining PT entries stay zero. */
    for(unsigned i=0;i<NESTED_EPT_MAP_PAGES;i++)ptr(c->tables[3])[i]=next.leaves[i];
    c->published=true;c->poisoned=true;
    if(!nested_ept_map_hw_flush(c))return false;
    c->committed=next;c->generation++;c->poisoned=false;return true;
}
bool nested_ept_map_hw_init(struct nested_ept_map_hw *c,struct ept_fixture *f) {
    *c=(struct nested_ept_map_hw){0};
    for(unsigned i=0;i<4;i++) {
        c->tables[i]=pmm_alloc_page();
        if(!c->tables[i]||!vmx_memory_region_wb(c->tables[i])||!ptr(c->tables[i]))return false;
        for(unsigned j=0;j<512;j++)ptr(c->tables[i])[j]=0;
    }
    for(unsigned i=0;i<3;i++)ptr(c->tables[i])[0]=c->tables[i+1]|7;
    c->eptp=c->tables[0]|0x1e;
    return nested_ept_map_hw_rebuild(c,f);
}
void nested_ept_map_hw_destroy(struct nested_ept_map_hw *c) {
    /* VMCS02 must already be CLEAR. Persistent flush failure retains pages. */
    if(c->published&&!nested_ept_map_hw_flush(c))panic("NESTED MAP INVEPT FAILED RETAIN PAGES");
    for(unsigned i=0;i<4;i++)if(c->tables[i]&&!pmm_free_page(c->tables[i]))panic("NESTED MAP FREE FAILED");
    *c=(struct nested_ept_map_hw){0};
}
