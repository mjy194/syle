#include "ept.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
static uint64_t *ptr(uint64_t p) { return paging_phys_to_virt(p); }
static bool alloc(uint64_t *p) {
    *p=pmm_alloc_page();
    if (!*p || !vmx_memory_region_wb(*p)) return false;
    uint64_t *v=ptr(*p);
    if (!v) return false;
    for (unsigned i=0;i<512;i++) v[i]=0;
    return true;
}
static uint64_t leaf(uint64_t p,unsigned permissions) { return p|permissions|(6u<<3); }
bool ept_fixture_flush(const struct ept_fixture *f) {
#ifdef EPT_HOST_TEST
    extern bool ept_test_invalidate(uint64_t);
    return ept_test_invalidate(f->eptp);
#else
    struct { uint64_t eptp,reserved; } descriptor __attribute__((aligned(16)))={f->eptp,0};
    unsigned char fail;
    uint64_t type=1;
    __asm__ volatile("invept %1,%2; setna %0":"=qm"(fail):"m"(descriptor),"r"(type):"cc","memory");
    return !fail;
#endif
}
static void release_owned(struct ept_fixture *f,bool check_total) {
    if(!f||!f->started) return;
    for (unsigned i=0;i<EPT_GUEST_PAGES;i++) if(f->pages[i]) {
        if (!pmm_free_page(f->pages[i])) panic("EPT GUEST FREE FAILED");
        f->pages[i]=0;
    }
    for (unsigned i=0;i<4;i++) if(f->tables[i]) {
        if (!pmm_free_page(f->tables[i])) panic("EPT TABLE FREE FAILED");
        f->tables[i]=0;
    }
    f->eptp=0;
    if(check_total && pmm_get_stats().allocated_pages != f->allocated_before) panic("EPT ALLOCATION ACCOUNTING FAILED");
    f->started=false;
}
void ept_fixture_destroy(struct ept_fixture *f) { release_owned(f,true); }
void ept_fixture_owned_release(struct ept_fixture *f) { release_owned(f,false); }
bool ept_fixture_init(struct ept_fixture *f) {
    if(!ept_fixture_prepare(f)) return false;
    if(ept_fixture_flush(f)) return true;
    ept_fixture_destroy(f);
    return false;
}
bool ept_fixture_prepare(struct ept_fixture *f) {
    if(!f) return false;
    *f=(struct ept_fixture){.allocated_before=pmm_get_stats().allocated_pages,.started=true};
    for (unsigned i=0;i<4;i++) if(!alloc(&f->tables[i])) goto fail;
    for (unsigned i=0;i<EPT_GUEST_PAGES;i++) if(!alloc(&f->pages[i])) goto fail;
    /* Strict 4-KiB leaves, only explicitly allocated guest backing admitted. */
    for (unsigned i=0;i<3;i++) ptr(f->tables[i])[0]=f->tables[i+1]|7;
    for (unsigned i=1;i<=EPT_GUEST_PAGES;i++) ptr(f->tables[3])[i]=leaf(f->pages[i-1],3);
    ptr(f->tables[3])[6]=leaf(f->pages[5],5); /* code RX */
    ptr(f->tables[3])[9]=0; /* deliberately absent despite allocated backing */
    ptr(f->tables[3])[10]=leaf(f->pages[9],1); /* deliberate write denial */
    ptr(f->tables[3])[11]=leaf(f->pages[10],1); /* deliberate execute denial */
#ifdef VMX_STACKPART_TEST
    for(unsigned page=12;page<=13;page++)
        for(unsigned i=0;i<512;i++)
            ptr(f->pages[page-1])[i]=UINT64_C(0x5a00000000000000)|((uint64_t)page<<20)|i;
#endif
#ifdef VMX_NESTED_TEST
    /* All L1/L2 RAM is L0-owned; no deliberately absent EPT test leaves. */
    for(unsigned i=9;i<=11;i++) ptr(f->tables[3])[i]=leaf(f->pages[i-1],3);
#endif
    f->eptp=f->tables[0]|6|(3u<<3); /* WB, four levels, no A/D */
    /* Guest owns all four paging levels. No host CR3, stack or GDT mappings. */
    ptr(f->pages[0])[0]=0x2000|3;
    ptr(f->pages[1])[0]=0x3000|3;
    ptr(f->pages[2])[0]=0x4000|3;
    for (unsigned i=1;i<=EPT_GUEST_PAGES;i++) ptr(f->pages[3])[i]=(i*4096)|3;
#if defined(VMX_NESTED_MEMORY_TEST)
    ptr(f->pages[3])[48]=UINT64_C(0xf063); /* PAT index0 WB */
#ifdef VMX_NESTED_UC_PAT_TEST
    ptr(f->pages[3])[49]=UINT64_C(0xf06b);
    ptr(f->pages[3])[50]=UINT64_C(0xf073);
#elif defined(VMX_NESTED_UC_TEST)
    ptr(f->pages[3])[49]=UINT64_C(0xe06b);
#else
    ptr(f->pages[3])[49]=UINT64_C(0xf06b);
#endif /* PAT index1 UC; EPT IPAT ignores */
#elif defined(VMX_NESTED_EPT_LARGE_TEST) || defined(VMX_NESTED_EPT_AD_TEST)
    ptr(f->pages[3])[48]=UINT64_C(0x208063);
    ptr(f->pages[3])[49]=UINT64_C(0x40009063);
#elif defined(VMX_NESTED_EPT_SPARSE_TEST)
    ptr(f->pages[3])[48]=UINT64_C(0xf063);
    ptr(f->pages[3])[49]=UINT64_C(0x201063);
    ptr(f->pages[3])[50]=UINT64_C(0x40001063);
    ptr(f->pages[3])[51]=UINT64_C(0x8000001063);
#endif
#ifdef VMX_NESTED_EPT_MAP_TEST
    ptr(f->pages[3])[20]=0x14003;
    ptr(f->pages[3])[21]=0x15003;
#endif
#ifdef VMX_IOAPIC_TEST
    /* Experimental MMIO aperture: valid guest mapping, no EPT RAM leaf. */
    ptr(f->pages[3])[14]=0xe003;
#endif
    #ifdef VMX_PF_TEST
    /* GVA c000 has a nonpresent guest PTE targeting EPT-backed GPA 8000.
     * All page-walk GPAs remain EPT RW. This is NOT EPT permission denial. */
    ptr(f->pages[3])[12]=0x8002;
#endif
#ifdef VMX_NESTED_PF_INJECT_TEST
    /* Nonpresent guest translation, but destination and all page tables have
     * valid EPT backing. This must be #PF, not an EPT violation. */
    ptr(f->pages[3])[15]=0x8002;
#endif
    uint64_t *gdt=ptr(f->pages[4]);
    gdt[1]=0x00af9b000000ffffULL;gdt[2]=0x00cf93000000ffffULL;
    gdt[3]=103ULL|(0x5100ULL<<16)|(0x8bULL<<40);gdt[4]=0;
    ((unsigned char *)gdt)[0x100+102]=104; /* TSS I/O map beyond limit */
    #ifdef VMX_EVENT_TEST
    extern const unsigned char ept_ud_handler[];
    uint64_t handler=EPT_CODE_GPA+(uintptr_t)ept_ud_handler-(uintptr_t)ept_payload_start;
    uint64_t *gate=(uint64_t *)((unsigned char *)gdt+0x800+6*16);
    gate[0]=(handler&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((handler&0xffff0000ULL)<<32);
    gate[1]=handler>>32;
#ifdef VMX_GP_TEST
    extern const unsigned char ept_gp_handler[];
    handler=EPT_CODE_GPA+(uintptr_t)ept_gp_handler-(uintptr_t)ept_payload_start;
    gate=(uint64_t *)((unsigned char *)gdt+0x800+13*16);
    gate[0]=(handler&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((handler&0xffff0000ULL)<<32);
    gate[1]=handler>>32;
#endif
#ifdef VMX_PF_TEST
    extern const unsigned char ept_pf_handler[];
    handler=EPT_CODE_GPA+(uintptr_t)ept_pf_handler-(uintptr_t)ept_payload_start;
    gate=(uint64_t *)((unsigned char *)gdt+0x800+14*16);
    gate[0]=(handler&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((handler&0xffff0000ULL)<<32);
    gate[1]=handler>>32;
#endif
#ifdef VMX_WAKE_TEST
    extern const unsigned char ept_irq_handler[];
    handler=EPT_CODE_GPA+(uintptr_t)ept_irq_handler-(uintptr_t)ept_payload_start;
    gate=(uint64_t *)((unsigned char *)gdt+0x800+0x40*16);
    gate[0]=(handler&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((handler&0xffff0000ULL)<<32);
    gate[1]=handler>>32;
#endif
#endif
#ifdef VMX_APIC_TEST
    extern const unsigned char ept_apic_handler[];
    uint64_t handler=EPT_CODE_GPA+(uintptr_t)ept_apic_handler-(uintptr_t)ept_payload_start;
    uint64_t *gate=(uint64_t *)((unsigned char *)gdt+0x800+0x40*16);
    gate[0]=(handler&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((handler&0xffff0000ULL)<<32);
    gate[1]=handler>>32;
#endif
#ifdef VMX_COLLIDE_DELIVER_TEST
    extern const unsigned char ept_collision_ud_handler[];
    uint64_t udhandler=EPT_CODE_GPA+(uintptr_t)ept_collision_ud_handler-(uintptr_t)ept_payload_start;
    uint64_t *udgate=(uint64_t *)((unsigned char *)gdt+0x800+6*16);
    udgate[0]=(udhandler&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((udhandler&0xffff0000ULL)<<32);
    udgate[1]=udhandler>>32;
#endif
#ifdef VMX_NESTED_EVENT_TEST
    extern const unsigned char nested_ud_handler[];
    uint64_t nh=EPT_CODE_GPA+(uintptr_t)nested_ud_handler-(uintptr_t)ept_payload_start;
    uint64_t *ng=(uint64_t *)((unsigned char *)gdt+0x800+6*16);
    ng[0]=(nh&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((nh&0xffff0000ULL)<<32);
    ng[1]=nh>>32;
#ifdef VMX_NESTED_GP_INJECT_TEST
    extern const unsigned char nested_gp_handler[];
    nh=EPT_CODE_GPA+(uintptr_t)nested_gp_handler-(uintptr_t)ept_payload_start;
#ifdef VMX_NESTED_PF_INJECT_TEST
    ng=(uint64_t *)((unsigned char *)gdt+0x800+14*16);
#else
    ng=(uint64_t *)((unsigned char *)gdt+0x800+13*16);
#endif
    ng[0]=(nh&0xffff)|(8ULL<<16)|(0x8eULL<<40)|((nh&0xffff0000ULL)<<32);
    ng[1]=nh>>32;
#endif
#endif
    uintptr_t n=(uintptr_t)ept_payload_end-(uintptr_t)ept_payload_start;
    uintptr_t x=(uintptr_t)ept_exec_end-(uintptr_t)ept_exec_start;
    if(n>4096 || x>4096) goto fail;
    unsigned char *code=(void *)ptr(f->pages[5]);
    for(uintptr_t i=0;i<n;i++) code[i]=ept_payload_start[i];
    code=(void *)ptr(f->pages[10]);
    for(uintptr_t i=0;i<x;i++) code[i]=ept_exec_start[i];
    ptr(f->pages[8])[0]=0x12345678;
    return true;
fail:
    ept_fixture_owned_release(f);
    return false;
}
bool ept_fixture_remediate(struct ept_fixture *f,unsigned phase) {
    if(phase>=3 || !f->started || !f->eptp) return false;
    unsigned index=9+phase;
    ptr(f->tables[3])[index]=leaf(f->pages[index-1],phase==2?5:3);
    /* Do not retry the faulting instruction until translation invalidation succeeds. */
    return ept_fixture_flush(f);
}

bool ept_fixture_data_ok(const struct ept_fixture *f) {
    return ptr(f->pages[7])[0]==0x42 && ptr(f->pages[8])[0]==0x12345678 &&
           ptr(f->pages[9])[0]==0x77;
}

bool ept_fixture_demand_ok(const struct ept_fixture *f) {
    /* Guest PTE changes only; preexisting EPT leaf and backing remain intact. */
    return ept_demand_pte_valid(ptr(f->pages[3])[12]) &&
           ptr(f->tables[3])[8]==leaf(f->pages[7],3) &&
           ptr(f->pages[7])[0]==0x42;
}

bool ept_fixture_movss_remediate(struct ept_fixture *f) {
    if(ptr(f->tables[3])[11]!=leaf(f->pages[10],5)) return false;
    ptr(f->tables[3])[11]=leaf(f->pages[10],7);
    return ept_fixture_flush(f);
}
bool ept_fixture_movss_ok(const struct ept_fixture *f) {
    return ptr(f->tables[3])[11]==leaf(f->pages[10],7) &&
           ptr(f->pages[10])[0x80/8]==0x55;
}

bool ept_fixture_idt_access(struct ept_fixture *f,bool allow) {
    uint64_t old=ptr(f->tables[3])[5],normal=leaf(f->pages[4],3);
    if(old!=(allow?0:normal)) return false;
    ptr(f->tables[3])[5]=allow?normal:0;
    return ept_fixture_flush(f);
}
bool ept_vectoring_gp_valid(uint64_t reason,uint64_t q,uint64_t gpa,
    uint64_t info,uint64_t error,uint64_t rip,uint64_t expected,
    uint32_t *entry_info,uint32_t *entry_error) {
    /* IDT-vectoring bit12 is undefined, unlike entry's reserved bit12.
     * Accept no other event/reserved bits and reconstruct clean entry info.
     * EPT read with zero R/W/X permissions, within exactly gate13's bytes. */
    if(reason!=48 || (q&0x3f)!=1 || gpa<0x58d0 || gpa>0x58df ||
       (info&~UINT64_C(0x1000))!=UINT64_C(0x80000b0d) ||
       error!=0x28 || rip!=expected) return false;
    *entry_info=0x80000b0d;*entry_error=(uint32_t)error;return true;
}

#ifdef VMX_STACKPART_TEST
static uint64_t stack_sentinel(unsigned page,unsigned i) {
    return UINT64_C(0x5a00000000000000)|((uint64_t)page<<20)|i;
}
bool ept_stackpart_protect(struct ept_fixture *f,bool protect) {
    uint64_t normal=leaf(f->pages[11],3),ro=leaf(f->pages[11],1);
    if(ptr(f->tables[3])[12]!=(protect?normal:ro)) return false;
    ptr(f->tables[3])[12]=protect?ro:normal;
    return ept_fixture_flush(f);
}
int ept_stackpart_observe(const struct ept_fixture *f,uint64_t flags) {
    const uint64_t expected[4]={8,flags|(1u<<16),0xd020,16};
    int changed=0;
    for(unsigned i=0;i<512;i++) {
        if(ptr(f->pages[11])[i]!=stack_sentinel(12,i)) return -1;
        uint64_t value=ptr(f->pages[12])[i];
        if(value==stack_sentinel(13,i)) continue;
        if(i>=4 || value!=expected[i]) return -1;
        changed++;
    }
    return changed; /* zero is NOT_EXERCISED, not success */
}
bool ept_stackpart_complete(const struct ept_fixture *f,uint64_t flags,uint64_t return_rip) {
    const uint64_t expected[4]={8,flags|(1u<<16),0xd020,16};
    for(unsigned i=0;i<512;i++) {
        uint64_t lower=i==510?0x28:i==511?return_rip:stack_sentinel(12,i);
        uint64_t upper=i<4?expected[i]:stack_sentinel(13,i);
        if(ptr(f->pages[11])[i]!=lower || ptr(f->pages[12])[i]!=upper) return false;
    }
    return true;
}
#endif
