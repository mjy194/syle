#include "nested_ept_hw.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
static uint64_t *ptr(uint64_t p) {return paging_phys_to_virt(p);}
static bool resolve(void *ctx,uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *perm) {
    (void)need;struct ept_fixture *f=ctx;
    if((gpa&4095)||gpa<4096||gpa>=((uint64_t)EPT_GUEST_PAGES+1)*4096)return false;
    unsigned page=(unsigned)(gpa>>12);
    /* EPT01 is this immutable private identity fixture. Validate its real leaf
     * rather than treating the L1 GPA as a host address. */
    uint64_t leaf=ptr(f->tables[3])[page];
    if((leaf&UINT64_C(0xffffffffff000))!=f->pages[page-1]||!(leaf&7))return false;
    *hpa=f->pages[page-1];*view=ptr(*hpa);*perm=leaf&7;
    return *view!=0;
}
bool nested_ept_hw_flush(struct nested_ept_hw *c) {
    struct {uint64_t p,reserved;} d __attribute__((aligned(16)))={c->eptp,0};
    uint64_t type=1;unsigned char fail;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(d),"r"(type):"cc","memory");return !fail;
}
bool nested_ept_hw_rebuild(struct nested_ept_hw *c,struct ept_fixture *f) {
    struct nested_ept_result plan[16];
    for(unsigned i=1;i<=15;i++) {
        plan[i]=nested_ept_walk(0x1001e,(uint64_t)i*4096,1,resolve,f);
        if(plan[i].status!=NESTED_EPT_OK||!plan[i].leaf_resolved)return false;
    }
    /* All validation completes before publishing any new leaf. No guest runs
     * concurrently. An invalidation failure stops permanently, never reentry. */
    for(unsigned i=1;i<=15;i++)ptr(c->tables[3])[i]=(plan[i].hpa&~UINT64_C(4095))|plan[i].effective02|(6u<<3);
    c->published=true;
    if(!nested_ept_hw_flush(c))return false;
    for(unsigned i=1;i<=15;i++)c->plan[i]=plan[i];
    return true;
}
bool nested_ept_hw_init(struct nested_ept_hw *c,struct ept_fixture *f) {
    *c=(struct nested_ept_hw){0};
    for(unsigned i=0;i<4;i++) {
        c->tables[i]=pmm_alloc_page();
        if(!c->tables[i]||!vmx_memory_region_wb(c->tables[i])||!ptr(c->tables[i]))return false;
        for(unsigned j=0;j<512;j++)ptr(c->tables[i])[j]=0;
    }
    for(unsigned i=0;i<3;i++)ptr(c->tables[i])[0]=c->tables[i+1]|7;
    c->eptp=c->tables[0]|0x1e;
    return nested_ept_hw_rebuild(c,f);
}
void nested_ept_hw_destroy(struct nested_ept_hw *c) {
    /* Caller first VMCLEARs physical VMCS02. Stale EPT translations are purged
     * before reuse/free, and failure retains all potentially cached pages. */
    if(c->published&&!nested_ept_hw_flush(c))panic("NESTED EPT02 INVEPT FAILED RETAIN PAGES");
    for(unsigned i=0;i<4;i++)if(c->tables[i]&&!pmm_free_page(c->tables[i]))panic("NESTED EPT02 FREE FAILED");
    *c=(struct nested_ept_hw){0};
}
