#include "cache_registered.h"
#include "cache_page.h"
#include "cache_lease_refs.h"
#include "nested_memory_type.h"
#include "nested_ept_contexts.h"
#include "smp_acpi.h"
#include "paging.h"
#include "memory.h"
#include "vmx_memory.h"
#include "nested_ept_sparse_hw.h"
#include "../efi/src/exceptions.h"
static struct cache_page_registry registry;
static struct cache_page_proof proof;
static bool platform_ready,pinned,live;
static uint64_t pinned_epoch;
static bool pat_enabled;
static uint64_t approved_pat;
static uint8_t destination_type;
static unsigned destination_slot;
static uint64_t destination_bits,original_pat;
static enum cache_page_phase typed_phase(void){return destination_type==4?CACHE_PAGE_WT:CACHE_PAGE_UC;}
static uint64_t typed_leaf(void){return ((uint64_t)destination_type<<3)|0x43;}
static struct cache_lease_refs references;
static struct ept_fixture *owned;
static uint64_t msr(unsigned n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return a|((uint64_t)d<<32);}
static uint64_t *view(uint64_t p){return paging_phys_to_virt(p);}
static void require(bool ok){if(!ok){cache_page_fail(&registry);panic("REGISTERED UC FAILED RETAIN NO REENTRY");}}
static bool read_physical(void *ctx,uint64_t pa,void *dst,size_t n) {
    const struct syle_boot_info *b=ctx;
    if(!n||pa>UINT64_MAX-n)return false;
    for(uint64_t off=0;off<b->memory_map_bytes;off+=b->descriptor_size) {
        const unsigned char *d=(void *)(uintptr_t)(b->memory_map+off);
        uint32_t type;uint64_t base,pages,attr;
        __builtin_memcpy(&type,d,4);__builtin_memcpy(&base,d+8,8);
        __builtin_memcpy(&pages,d+24,8);__builtin_memcpy(&attr,d+32,8);
        if(!((type>=1&&type<=7)||type==9||type==10||type==14)||!(attr&9)||
           (attr&0x2000)||pages>UINT64_MAX/4096||base>UINT64_MAX-pages*4096)continue;
        if(pa>=base&&pa+n<=base+pages*4096) {
            const unsigned char *s=paging_phys_to_virt(pa);
            if(!s||!paging_phys_to_virt(pa+n-1))return false;
            unsigned char *to=dst;for(size_t i=0;i<n;i++)to[i]=s[i];return true;
        }
    }
    return false;
}
static bool table_read(void *ctx,uint64_t hpa,const uint64_t **out){
    (void)ctx;
    if(!hpa||(hpa&4095)||hpa>=UINT64_C(0x100000000)||!view(hpa))return false;
    *out=view(hpa);return true; /* own immutable root's low table allocation contract */
}

bool cache_registered_platform_type(const struct syle_boot_info *boot,uint8_t type){
    uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    if(!(c&(1u<<5))||!(d&(1u<<16))||(type!=0&&type!=4))return false;
    uint64_t pat=msr(0x277),bits;unsigned slot;
    if(!cache_page_pat_slot(pat,type,&slot,&bits))return false;
    struct smp_acpi_topology top;
    if(platform_ready||paging_ap_start_attempted()||!smp_acpi_discover_single(boot->acpi_rsdp,b>>24,read_physical,(void *)boot,&top))return false;
    uint64_t basic=msr(0x480),primary=msr((basic&(UINT64_C(1)<<55))?0x48e:0x482);
    uint64_t secondary=(primary&(UINT64_C(1)<<63))?msr(0x48b):0;
    uint64_t caps=(secondary&(UINT64_C(1)<<33))?msr(0x48c):0;
    if(!cache_uc_vmx_capable(primary,secondary,caps))return false;
    proof=(struct cache_page_proof){true,top.enabled_count==1&&!paging_ap_start_attempted(),true,true,true};
    destination_type=type;destination_slot=slot;destination_bits=bits;original_pat=pat;
    platform_ready=true;return true;
}
bool cache_registered_platform(const struct syle_boot_info *boot){return cache_registered_platform_type(boot,0);}
static bool flush(void){struct {uint64_t p,z;} x={owned->eptp,0};uint64_t type=1;unsigned char fail;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(x),"r"(type):"cc","memory");return !fail;}
static void detach(uint64_t expected){
    unsigned count=0;for(unsigned i=0;i<3;i++)require(view(owned->tables[i])[0]==(owned->tables[i+1]|7));
    uint64_t *pt=view(owned->tables[3]);
    for(unsigned i=0;i<512;i++)if((pt[i]&7)&&(pt[i]&UINT64_C(0x000ffffffffff000))==registry.lease.target_hpa){require(i==15&&pt[i]==expected);count++;}
    require(count==1);__atomic_store_n(&pt[15],0,__ATOMIC_RELEASE);require(flush());
}
static void transition(enum cache_page_phase destination,uint64_t eptleaf){
    require(!pinned&&!references.refs);uint64_t ticket;enum cache_page_phase old=registry.phase;
    require(cache_page_begin(&registry,destination,&proof,&ticket));detach(eptleaf);
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_DETACH_GUEST_INVEPT));
    uint64_t *pte=&view(registry.lease.table_hpa)[registry.lease.index];
    uint64_t expected=(registry.lease.oldraw&~UINT64_C(0x98))|(old==typed_phase()?destination_bits:0);
    require(((*pte^expected)&~UINT64_C(0x60))==0);
    __atomic_store_n(pte,0,__ATOMIC_RELEASE);require(cache_page_ack(&registry,ticket,CACHE_PAGE_HOST_NONPRESENT));
    __asm__ volatile("invlpg (%0)"::"r"(registry.lease.target_hpa):"memory");require(cache_page_ack(&registry,ticket,CACHE_PAGE_TLB_BEFORE));
    __asm__ volatile("wbinvd":::"memory");require(cache_page_ack(&registry,ticket,CACHE_PAGE_CACHE_BEFORE));
    uint64_t next=(registry.lease.oldraw&~UINT64_C(0x98))|(destination==typed_phase()?destination_bits:0);
    __atomic_store_n(pte,next,__ATOMIC_RELEASE);require(cache_page_ack(&registry,ticket,CACHE_PAGE_INSTALL));
    __asm__ volatile("invlpg (%0)"::"r"(registry.lease.target_hpa):"memory");require(cache_page_ack(&registry,ticket,CACHE_PAGE_TLB_AFTER));
    __asm__ volatile("wbinvd":::"memory");require(cache_page_ack(&registry,ticket,CACHE_PAGE_CACHE_AFTER)&&cache_page_commit(&registry,ticket));
}
void cache_registered_begin(struct ept_fixture *f){
    uint64_t flags,cr0,cr3,cr4;
    __asm__ volatile("pushfq;popq %0":"=r"(flags));__asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));__asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    require(platform_ready&&!live&&!pinned&&f&&f->started&&!paging_ap_start_attempted()&&!(flags&0x200)&&cr3==paging_root()&&
        !(cr0&0x60000000)&&!(cr4&((1u<<7)|(1u<<12)|(1u<<17))));
    owned=f;uint64_t target=f->pages[14];
    require(target&&vmx_memory_region_wb(target));
    struct cache_page_lease lease;
    require(cache_page_audit(cr3,target,msr(0x277),8192,table_read,0,&lease)==CACHE_PAGE_AUDIT_OK&&cache_page_init(&registry,&lease,&proof));
    require(msr(0x277)==original_pat);
    log_text("NESTED.TYPED_MTRR_WB_PROVED type=");log_hex(destination_type);
    log_text("NESTED.TYPED_HOST_PAT=");log_hex(original_pat);log_text("NESTED.TYPED_HOST_SLOT=");log_hex(destination_slot);
    log_text("NESTED.TYPED_HOST_CACHE_BITS=");log_hex(destination_bits);
    transition(typed_phase(),target|0x33);
    __atomic_store_n(&view(f->tables[3])[15],target|typed_leaf(),__ATOMIC_RELEASE);require(flush());live=true;
    require(cache_lease_refs_init(&references,registry.generation));
}
void cache_registered_pin(void){require(live&&!pinned&&registry.phase==typed_phase());pinned=true;pinned_epoch=registry.generation;}
void cache_registered_unpin_after_destroy(void){require(live&&pinned);pinned=false;}
void cache_registered_finish(struct ept_fixture *f){
    require(live&&f==owned&&!pinned&&!references.refs);uint64_t current;
    unsigned char fail;__asm__ volatile("vmptrst %1;setna %0":"=qm"(fail),"=m"(current)::"cc","memory");
    require(!fail&&current==UINT64_MAX); /* both VMCS01/02 already CLEAR */
    transition(CACHE_PAGE_WB,registry.lease.target_hpa|typed_leaf());
    require(cache_page_lease_matches(&registry.lease,view(registry.lease.table_hpa)[registry.lease.index]));live=false;
    require(msr(0x277)==original_pat&&vmx_memory_region_wb(registry.lease.target_hpa));
    if(destination_type==4)log_text("NESTED.WT_LEASE_RESTORED_WB PAT_unchanged=1 all_EPT_flushed=1\n");
    else log_text("NESTED.UC_LEASE_RESTORED_WB all_VMCS_clear=1 all_EPT_flushed=1\n");
}
bool cache_registered_admit(void *ctx,const struct nested_ept_result *r){
    if(ctx!=owned||!r||!live||(!pinned&&!references.refs)||registry.phase!=typed_phase()||registry.generation!=(pinned?pinned_epoch:references.generation)||
       !r->memory_explicit||r->walk_type!=6)return false;
    if(r->hpa==registry.lease.target_hpa)return r->leaf_type==destination_type&&r->l1_gpa==0xf000&&!(r->effective02&4)&&
        (r->ignore_pat||(destination_type==0&&pat_enabled&&nested_memory_backing_compatible(0,false,false,false,approved_pat,0)));
    if(!r->ignore_pat||r->leaf_type!=6||(r->l1_gpa&4095)||r->l1_gpa<4096||r->l1_gpa>=((uint64_t)EPT_GUEST_PAGES+1)*4096)return false;
    unsigned page=r->l1_gpa>>12;
    return r->hpa==owned->pages[page-1]&&
        (view(owned->tables[3])[page]&~UINT64_C(7))==(r->hpa|0x30);
    /* Only allocator-owned untransitioned WB pages can obtain this capability. */
}
bool cache_registered_resolve(void *ctx,uint64_t gpa,uint8_t need,uint64_t *hpa,const uint64_t **pointer,uint8_t *perm){
    if(ctx!=owned||!live||(!pinned&&!references.refs)||registry.phase!=typed_phase()||registry.generation!=(pinned?pinned_epoch:references.generation))return false;
    if(gpa!=0xf000)return nested_sparse_hw_resolve(ctx,gpa,need,hpa,pointer,perm);
    if((need&4)||view(owned->tables[3])[15]!=(registry.lease.target_hpa|typed_leaf()))return false;
    *hpa=registry.lease.target_hpa;*pointer=0;*perm=3;
    /* NULL view deliberately rejects this UC data page if used as EPT12 table. */
    return true;
}

/* Context token exists before typed publication and outlives every reset.
 * VMCS pins are independently enforced by the context registry. */
static bool context_admit(void *ctx,const struct nested_ept_result *r){
    struct nested_ept_context_item *item=ctx;
    struct cache_lease_token *token=&item->attachment.uc_token;
    return cache_lease_refs_admit(&references,token,item,registry.generation)&&cache_registered_admit(owned,r);
}
void cache_registered_context_lifecycle(void *ctx,struct nested_ept_context_item *item,enum nested_context_event event){
    require(ctx==owned&&live&&registry.phase==typed_phase());
    struct cache_lease_token *token=&item->attachment.uc_token;
    _Static_assert(sizeof(*token)<=sizeof(item->attachment),"UC token fits attachment");
    if(event==NESTED_CONTEXT_INIT){
        require(cache_lease_refs_acquire(&references,token,item,registry.generation));
        require(nested_ept_sparse_set_typed_admission(&item->tree,context_admit,item));
    }else if(event==NESTED_CONTEXT_RESET){
        require(cache_lease_refs_admit(&references,token,item,registry.generation));
    }else if(event==NESTED_CONTEXT_DESTROY){
        require(cache_lease_refs_release(&references,token,item,registry.generation));
    }else require(false);
}
uint64_t cache_registered_context_refs(void){return references.refs;}

bool cache_registered_pat_entry(uint64_t pat,uint64_t cr0){
    if(!live||destination_type!=0||registry.phase!=typed_phase()||
       !nested_memory_backing_compatible(0,false,(cr0&(1u<<30))!=0,(cr0&(1u<<29))!=0,pat,0))return false;
    approved_pat=pat;pat_enabled=true;return true;
}
