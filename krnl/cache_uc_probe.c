#include "cache_page.h"
#include "smp_acpi.h"
#include "paging.h"
#include "memory.h"
#include "vmx_memory.h"
#include "smp_vmx.h"
#include "smp_guest.h"
#include "smp_ept.h"
#include "../efi/src/exceptions.h"
static struct cache_page_registry registry;
static struct smp_ept fixture;
static struct smp_vmx_context rootctx;
static struct smp_guest_context guest;
extern const unsigned char smp_guest_uc_payload_start[],smp_guest_uc_payload_end[];
extern const uint32_t smp_guest_uc_payload_call_offset;
static struct cache_page_proof proof;
static void require(bool ok){if(!ok){cache_page_fail(&registry);panic("CACHE UC FAILURE RETAIN PAGE NO REENTRY");}}
static uint64_t msr(unsigned n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return a|((uint64_t)d<<32);}
static uint64_t *view(uint64_t p){return paging_phys_to_virt(p);}
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
static bool flush_ept(void){
    struct {uint64_t p,z;} d={fixture.eptp,0};uint64_t type=1;unsigned char fail;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(d),"r"(type):"cc","memory");return !fail;
}
static void detach_guest(uint64_t expected){
    for(unsigned i=0;i<3;i++)require(view(fixture.table[i])[0]==(fixture.table[i+1]|7));
    uint64_t *pt=view(fixture.table[3]);unsigned aliases=0;
    for(unsigned i=0;i<512;i++)if((pt[i]&7)&&((pt[i]&UINT64_C(0x000ffffffffff000))==fixture.page[7])){
        require(i==8&&pt[i]==expected);aliases++;
    }
    require(aliases==1);__atomic_store_n(&pt[8],0,__ATOMIC_RELEASE);
    require(flush_ept());
}
/* Only this dedicated, owned, never-DMA page is inaccessible in the gap.
 * No guest or AP runs. PAT/MTRR/CR0.CD are not changed. */
static void transition(enum cache_page_phase destination,uint64_t old_ept){
    uint64_t ticket;enum cache_page_phase old=registry.phase;
    require(!guest.vmcs_live&&cache_page_begin(&registry,destination,&proof,&ticket));
    detach_guest(old_ept);
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_DETACH_GUEST_INVEPT));
    uint64_t *pte=&view(registry.lease.table_hpa)[registry.lease.index];
    uint64_t expected=(registry.lease.oldraw&~UINT64_C(24))|(old==CACHE_PAGE_UC?24:0);
    require(((*pte^expected)&~UINT64_C(0x60))==0);
    __atomic_store_n(pte,0,__ATOMIC_RELEASE);
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_HOST_NONPRESENT));
    __asm__ volatile("invlpg (%0)"::"r"(registry.lease.target_hpa):"memory");
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_TLB_BEFORE));
    __asm__ volatile("wbinvd":::"memory");
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_CACHE_BEFORE));
    uint64_t next=(registry.lease.oldraw&~UINT64_C(24))|(destination==CACHE_PAGE_UC?24:0);
    __atomic_store_n(pte,next,__ATOMIC_RELEASE);
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_INSTALL));
    __asm__ volatile("invlpg (%0)"::"r"(registry.lease.target_hpa):"memory");
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_TLB_AFTER));
    __asm__ volatile("wbinvd":::"memory");
    require(cache_page_ack(&registry,ticket,CACHE_PAGE_CACHE_AFTER)&&cache_page_commit(&registry,ticket));
    require(((*pte^next)&~UINT64_C(0x60))==0);
}
void cache_uc_probe(const struct syle_boot_info *boot){
    uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    if(!(c&(1u<<5))){log_text("CACHE.N26_BLOCKED CPUID_VMX0\n");return;}
    /* CPUID reports package capacity, not the VM-wide enabled CPU count.
     * Keep it as evidence only; MADT+no-start proof below owns admission. */
    log_text("CACHE.CPUID_PACKAGE_CAPACITY=");log_hex((b>>16)&255);
    uint32_t owner=b>>24;
    struct smp_acpi_topology topology;
    if(!boot||paging_ap_start_attempted()||
       !smp_acpi_discover_single(boot->acpi_rsdp,owner,read_physical,(void *)boot,&topology)) {
        log_text("CACHE.N26_BLOCKED MADT_SINGLE_CPU_NO_AP_START_REQUIRED\n");return;
    }
    uint64_t basic=msr(0x480);
    uint64_t primary=msr((basic&(UINT64_C(1)<<55))?0x48e:0x482);
    uint64_t secondary=(primary&(UINT64_C(1)<<63))?msr(0x48b):0;
    uint64_t ept=(secondary&(UINT64_C(1)<<33))?msr(0x48c):0;
    if(!cache_uc_vmx_capable(primary,secondary,ept)) {
        log_text("CACHE.N26_BLOCKED EPT_INVEPT_CAPABILITY\n");return;
    }
    /* Proofs are derived from validated topology and this executable's
     * exclusive fresh allocation/no-device/no-guest-running ownership. */
    proof=(struct cache_page_proof){true,topology.enabled_count==1&&!paging_ap_start_attempted(),true,true,true};
    log_text("CACHE.SINGLE_CPU_MADT_VALIDATED no_AP_start=1 EPT_INVEPT_gated=1\n");
    uint64_t flags,cr3,cr4,cr0;
    __asm__ volatile("pushfq;popq %0":"=r"(flags));__asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));__asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    require(!(flags&0x200)&&cr3==paging_root()&&!(cr4&((1u<<12)|(1u<<17)|(1u<<7)))&&!(cr0&0x60000000));
    /* This executable never starts APs or programs any DMA device. Fresh
     * allocator pages exclude live code, stacks, boot objects and tables. */
    require(smp_ept_prepare(&fixture));
    unsigned bytes=smp_guest_uc_payload_end-smp_guest_uc_payload_start;
    require(bytes&&bytes<=4096);
    for(unsigned i=0;i<bytes;i++)((unsigned char *)view(fixture.page[5]))[i]=smp_guest_uc_payload_start[i];
    require(vmx_memory_region_wb(fixture.page[7]));
    struct cache_page_lease lease;
    require(cache_page_audit(cr3,fixture.page[7],msr(0x277),8192,table_read,0,&lease)==CACHE_PAGE_AUDIT_OK);
    require(cache_page_init(&registry,&lease,&proof));
    log_text("CACHE.AUDIT_SINGLE_HOST_ALIAS target=");log_hex(lease.target_hpa);
    rootctx.region_hpa=pmm_alloc_page_below(UINT64_C(0x100000000));rootctx.region_view=view(rootctx.region_hpa);rootctx.owner_apic_id=owner;
    guest.vmcs_hpa=pmm_alloc_page_below(UINT64_C(0x100000000));guest.vmcs_view=view(guest.vmcs_hpa);guest.owner_apic_id=owner;
    require(rootctx.region_hpa&&guest.vmcs_hpa&&rootctx.region_hpa!=guest.vmcs_hpa);
    require(smp_vmx_local_enter(&rootctx)==SMP_VMX_OK);
    transition(CACHE_PAGE_UC,fixture.page[7]|0x31);
    uint64_t uc=fixture.page[7]|0x43; /* UC leaf+IPAT, RW. Host PAT3 is UC too. */
    __atomic_store_n(&view(fixture.table[3])[8],uc,__ATOMIC_RELEASE);require(flush_ept());
    guest.eptp=fixture.eptp;guest.stack_top=0xa000;guest.expected_data=0x77;guest.owner_token=0xc026;
    guest.vmcall_gpa=0x6000+smp_guest_uc_payload_call_offset;
    require(smp_guest_local_run(&guest));
    require(guest.frame.r10==UINT64_C(0x1122334455667788)&&*(volatile uint64_t *)view(fixture.page[7])==0x77);
    log_text("CACHE.UC_GUEST_READ_WRITE_PASS host_PAT3_UC=1 EPT_UC_IPAT=1\n");
    transition(CACHE_PAGE_WB,uc);
    require(smp_vmx_local_leave(&rootctx)==SMP_VMX_OK);
    require(*(volatile uint64_t *)view(fixture.page[7])==0x77&&cache_page_lease_matches(&lease,*(&view(lease.table_hpa)[lease.index])));
    require(pmm_free_page(guest.vmcs_hpa)&&pmm_free_page(rootctx.region_hpa));
    smp_ept_release(&fixture);
    log_text("CACHE.N26_PASS actual_UC_guest=1 break_before_make=2 restored_WB_before_free=1 single_CPU=1\n");
}
