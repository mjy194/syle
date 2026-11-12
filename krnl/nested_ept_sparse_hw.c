#include "nested_ept_sparse_hw.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
static uint64_t *ptr(uint64_t p){return paging_phys_to_virt(p);}
static bool allocate(void *ctx,struct nested_sparse_page *page) {
    struct nested_ept_sparse_hw *c=ctx;
    /* Resource budget, not a GPA window: high sparse addresses cost only the
     * actual nodes/metadata they need. No untrusted unlimited allocation. */
    if(c->allocated_pages>=256)return false;
    uint64_t p=pmm_alloc_page();if(!p)return false;
    if(!vmx_memory_region_wb(p)||!ptr(p)) {
        if(!pmm_free_page(p))panic("SPARSE ALLOC ROLLBACK FAILED");
        return false;
    }
    *page=(struct nested_sparse_page){p,ptr(p)};c->allocated_pages++;return true;
}
static void release(void *ctx,struct nested_sparse_page page) {
    struct nested_ept_sparse_hw *c=ctx;
    if(!c->allocated_pages||!pmm_free_page(page.hpa))panic("SPARSE FREE FAILED");
    c->allocated_pages--;
}
static bool flush(void *ctx,uint64_t eptp) {
    (void)ctx;
    struct {uint64_t p,reserved;} d __attribute__((aligned(16)))={eptp,0};
    uint64_t type=1;unsigned char fail;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(d),"r"(type):"cc","memory");return !fail;
}
struct nested_sparse_ops nested_sparse_hw_ops(void){return (struct nested_sparse_ops){allocate,release,flush};}
bool nested_sparse_hw_resolve(void *ctx,uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **view,uint8_t *perm) {
    (void)need;struct ept_fixture *f=ctx;
    if((gpa&4095)||gpa<4096||gpa>=((uint64_t)EPT_GUEST_PAGES+1)*4096)return false;
    for(unsigned i=0;i<3;i++)if(!ptr(f->tables[i])||ptr(f->tables[i])[0]!=(f->tables[i+1]|7))return false;
    if(!ptr(f->tables[3]))return false;
    unsigned page=gpa>>12;
    uint64_t leaf=ptr(f->tables[3])[page];
    if((leaf&~UINT64_C(7))!=(f->pages[page-1]|0x30)||!(leaf&7))return false;
    *hpa=f->pages[page-1];*view=ptr(*hpa);*perm=leaf&7;return *view!=0;
}
bool nested_ept_sparse_hw_init(struct nested_ept_sparse_hw *c,struct ept_fixture *f,unsigned bits) {
    *c=(struct nested_ept_sparse_hw){.fixture=f};
    struct nested_sparse_ops ops={allocate,release,flush};
    bool ok=nested_ept_sparse_init(&c->tree,&ops,c,bits);
    c->eptp=c->tree.eptp;return ok;
}
enum nested_sparse_fault nested_sparse_hw_tree_fault_policy(struct nested_ept_sparse_hw *c,
    struct nested_ept_sparse *tree,uint64_t eptp12,uint64_t gpa,uint8_t access,struct nested_ept_result *out,const struct nested_ept_policy *policy) {
    struct nested_ept_result r;
    enum nested_sparse_lookup found=nested_ept_sparse_lookup(tree,gpa,access,&r);
    if(found==NESTED_SPARSE_HIT) {
        *out=r;
        if(r.status==NESTED_EPT_L1_DENIED)return NS_FAULT_L1;
        if(r.status==NESTED_EPT_L0_BACKING_DENIED)return NS_FAULT_L0;
        return NS_FAULT_UNSUPPORTED; /* cached hardware-permitted fault: no loop */
    }
    if(found!=NESTED_SPARSE_MISS)return NS_FAULT_UNSUPPORTED;
    /* No negative cache: never-installed translations may see current EPT12.
     * Under no-exec-only/W=>R, read-walk obtains every legal present mapping. */
    r=policy?nested_ept_walk_policy(eptp12,gpa&~UINT64_C(4095),1,nested_sparse_hw_resolve,c->fixture,policy):
        nested_ept_walk(eptp12,gpa&~UINT64_C(4095),1,nested_sparse_hw_resolve,c->fixture);
    /* A failed L0 backing resolution is fatal even if another access bit
     * would also be denied by L1. Never relabel an L0 failure as L1's exit. */
    r=nested_sparse_walk_access(r,access);
    *out=r;
    if(r.status==NESTED_EPT_L1_DENIED)return NS_FAULT_L1;
    if(r.status==NESTED_EPT_L0_BACKING_DENIED)return NS_FAULT_L0;
    if(r.status!=NESTED_EPT_OK)return NS_FAULT_UNSUPPORTED;
    enum nested_sparse_fill result=nested_ept_sparse_fill(tree,gpa&~UINT64_C(4095),&r);
    return result==NESTED_SPARSE_OK?NS_FAULT_FILLED:NS_FAULT_UNSUPPORTED;
}
bool nested_ept_sparse_hw_reset(struct nested_ept_sparse_hw *c){return nested_ept_sparse_reset(&c->tree);}
void nested_ept_sparse_hw_destroy(struct nested_ept_sparse_hw *c) {
    if(!nested_ept_sparse_destroy(&c->tree))panic("SPARSE INVALIDATION FAILED RETAIN PAGES");
    if(c->allocated_pages)panic("SPARSE ALLOCATION LEAK");
    c->eptp=0;
}

enum nested_sparse_fault nested_ept_sparse_hw_fault(struct nested_ept_sparse_hw *c,uint64_t gpa,uint8_t access,struct nested_ept_result *out){return nested_sparse_hw_tree_fault(c,&c->tree,0x1001e,gpa,access,out);}

enum nested_sparse_fault nested_sparse_hw_tree_fault(struct nested_ept_sparse_hw *c,struct nested_ept_sparse *tree,uint64_t eptp12,uint64_t gpa,uint8_t access,struct nested_ept_result *out){return nested_sparse_hw_tree_fault_policy(c,tree,eptp12,gpa,access,out,0);}

/* Opt-in architectural classification; never relabel hardware EPT02 defects. */
enum nested_sparse_fault nested_sparse_hw_classified_fault(struct nested_ept_sparse_hw *c,
    uint64_t gpa,uint8_t access,struct nested_ept_result *out,const struct nested_ept_policy *policy) {
    struct nested_ept_result r;
    enum nested_sparse_lookup found=nested_ept_sparse_lookup(&c->tree,gpa,access,&r);
    if(found==NESTED_SPARSE_HIT){
        *out=r;
        if(r.status==NESTED_EPT_L1_DENIED)return NS_FAULT_L1;
        if(r.status==NESTED_EPT_L0_BACKING_DENIED)return NS_FAULT_L0;
        return NS_FAULT_UNSUPPORTED;
    }
    if(found!=NESTED_SPARSE_MISS)return NS_FAULT_UNSUPPORTED;
    r=nested_ept_walk_classified(0x1001e,gpa&~UINT64_C(4095),1,nested_sparse_hw_resolve,c->fixture,policy);
    r=nested_sparse_walk_access(r,access);*out=r;
    if(r.status==NESTED_EPT_L1_DENIED)return NS_FAULT_L1;
    if(r.status==NESTED_EPT_L0_BACKING_DENIED)return NS_FAULT_L0;
    if(r.status==NESTED_EPT_MISCONFIG)return NS_FAULT_MISCONFIG;
    if(r.status!=NESTED_EPT_OK)return NS_FAULT_UNSUPPORTED;
    return nested_ept_sparse_fill(&c->tree,gpa&~UINT64_C(4095),&r)==NESTED_SPARSE_OK?NS_FAULT_FILLED:NS_FAULT_UNSUPPORTED;
}
