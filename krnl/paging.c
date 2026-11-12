#include "paging.h"
#include "memory.h"
#include "../efi/src/exceptions.h"
#define PAGE UINT64_C(4096)
#define LOW_LIMIT UINT64_C(0x100000000)
#define DIRECT_BASE UINT64_C(0xffff800000000000)
#define HUGE UINT64_C(0x200000)
#define NX (UINT64_C(1)<<63)
#define ADDR UINT64_C(0x000ffffffffff000)
static uint64_t root;
static int ap_start_attempted;
void paging_note_ap_start(void){ap_start_attempted=1;}
int paging_ap_start_attempted(void){return ap_start_attempted;}
static unsigned tables;
extern char __kernel_start[], __text_end[], __rodata_start[], __rodata_end[], __kernel_end[];
uint64_t paging_root(void) { return root; }
void *paging_phys_to_virt(uint64_t p) {
    if (p >= pmm_address_limit()) return 0;
    return (void *)(uintptr_t)(p < LOW_LIMIT ? p : DIRECT_BASE + p);
}
static uint64_t new_table(void) {
    uint64_t p = pmm_alloc_page_below(LOW_LIMIT);
    if (!p) panic("PAGE TABLE ALLOCATION FAILED");
    uint64_t *t = (void *)(uintptr_t)p;
    for (unsigned i=0;i<512;++i) t[i]=0;
    ++tables;
    return p;
}
static uint64_t *entry(uint64_t va, int create, int level) {
    uint64_t *t=(void *)(uintptr_t)root;
    for (int shift=39;shift>level;shift-=9) {
        unsigned index=(va>>shift)&511;
        if (!(t[index]&1)) {
            if (!create) return 0;
            t[index]=new_table()|3;
        }
        if (t[index]&128) panic("PAGING UNEXPECTED HUGE LEAF");
        t=(void *)(uintptr_t)(t[index]&ADDR);
    }
    return &t[(va>>level)&511];
}
static uint64_t *leaf(uint64_t va, int create) { return entry(va,create,12); }
static void map_range(uint64_t base,uint64_t size,uint64_t flags) {
    if (!size) return;
    if (base >= pmm_address_limit() || size > pmm_address_limit()-base) panic("PAGING RANGE EXCEEDS ADDRESS LIMIT");
    uint64_t end=(base+size+PAGE-1)&~(PAGE-1);
    for (uint64_t p=base&~(PAGE-1);p<end;) {
        if (!p) { p+=PAGE; continue; } /* null page deliberately unmapped */
        uint64_t va=(uintptr_t)paging_phys_to_virt(p);
        /* High RAM is supervisor-only direct mapped, NX. Use 2 MiB leaves
         * only inside one descriptor; edge pages retain precise attributes. */
        if (p>=LOW_LIMIT && !(p&(HUGE-1)) && end-p>=HUGE) {
            *entry(va,1,21)=p|flags|129;
            p+=HUGE;
        } else { *leaf(va,1)=p|flags|1; p+=PAGE; }
    }
}
static void protect_range(uint64_t base,uint64_t size,uint64_t flags) {
    if (!size) return;
    if (base >= pmm_address_limit() || size > pmm_address_limit()-base) panic("PAGING RANGE EXCEEDS ADDRESS LIMIT");
    uint64_t end=(base+size+PAGE-1)&~(PAGE-1);
    for (uint64_t p=base&~(PAGE-1);p<end;p+=PAGE) {
        uint64_t *e=leaf(p,0);
        if (!e || !(*e&1)) panic("REQUIRED BOOT RAM NOT MAPPED");
        *e=p|flags|1|(*e&24); /* retain descriptor cache policy */
    }
}
/* Code publication is a one-way BSP operation. First remove write permission
 * from EVERY leaf covering the physical page, then discard old translations.
 * No executable mapping may be published by the caller until this succeeds.
 * Kernel/identity/direct-map aliases share this owned tree. The bootstrap
 * user tree inherits it only after sealing and never changes its aliases. */
#ifdef PAGING_HOST_TEST
extern int paging_test_context(void);
extern void paging_test_flush(void);
/* Observe hardware state without replacing the production permission walk. */
extern uint64_t paging_test_cr3(void);
extern uint64_t paging_test_cr4(void);
extern uint64_t paging_test_pat(void);
#endif
static int seal_context(void) {
#ifdef PAGING_HOST_TEST
    return paging_test_context();
#else
    uint64_t cr3,cr4,flags;
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    __asm__ volatile("pushfq;popq %0":"=r"(flags));
    return !(flags&512) && cr3==root && !(cr4&((UINT64_C(1)<<7)|(UINT64_C(1)<<12)|(UINT64_C(1)<<17)));
#endif
}
static void seal_flush(void) {
#ifdef PAGING_HOST_TEST
    paging_test_flush();
#else
    /* PGE and PCID are off: this includes aliases at other virtual addresses. */
    __asm__ volatile("mov %0,%%cr3"::"r"(root):"memory");
#endif
}
static unsigned seal_aliases(uint64_t table,int shift,uint64_t physical) {
    uint64_t *t=(void *)(uintptr_t)table;
    unsigned found=0;
    for(unsigned i=0;i<512;++i) {
        uint64_t e=t[i];
        if(!(e&1))continue;
        if(shift==12 || (e&128)) {
            if(shift!=12 && shift!=21 && shift!=30)
                panic("PAGING INVALID ALIAS LEAF");
            uint64_t size=UINT64_C(1)<<shift;
            uint64_t base=e&ADDR&~(size-1);
            if(physical<base || physical-base>=size)continue;
            if(shift==12) { t[i]=e&~UINT64_C(2); ++found; continue; }
            /* Split only the intersecting huge leaf, preserving permissions,
             * cache attributes (including PAT), and all adjacent frames. */
            uint64_t child=new_table(), *v=(void *)(uintptr_t)child;
            uint64_t flags=e&~ADDR;
            flags &= ~UINT64_C(128);
            if(shift==30)flags|=128|(e&4096);
            else if(e&4096)flags|=128; /* large PAT -> 4 KiB PAT */
            for(unsigned j=0;j<512;++j)
                v[j]=(base+((uint64_t)j<<(shift-9)))|flags;
            /* Cache policy belongs to leaves, not page-table accesses. */
            t[i]=child|7;
            found+=seal_aliases(child,shift-9,physical);
        } else found+=seal_aliases(e&ADDR,shift-9,physical);
    }
    return found;
}
int paging_seal_code_page(uint64_t physical) {
    if(!root || !physical || (physical&(PAGE-1)) ||
       physical>=pmm_address_limit() || ap_start_attempted || !seal_context())return 0;
    unsigned found=seal_aliases(root,39,physical);
    if(found)seal_flush();
    return found!=0;
}
static void require_flags(uint64_t p,uint64_t flags) {
    uint64_t *e=leaf(p,0);
    if (!e || (*e & (NX|7)) != (flags|1)) panic("PAGING PERMISSION SELFTEST FAILED");
}
void paging_init(const struct syle_boot_info *boot) {
    /* This bootstrap still passes identity pointers for loader objects and GOP.
     * Reject high placements before dropping firmware mappings, rather than
     * claiming that those raw pointers are high direct-map addresses. General
     * allocatable RAM has no such 4 GiB restriction. */
    struct syle_range low_objects[] = {
        {(uintptr_t)boot,sizeof(*boot)}, boot->image, boot->kernel_image,
        boot->map_buffer, boot->user_image_buffer,
        {boot->framebuffer.valid ? boot->framebuffer.base : 0,
         boot->framebuffer.valid ? boot->framebuffer.size : 0}
    };
    for (unsigned i=0;i<sizeof(low_objects)/sizeof(low_objects[0]);++i)
        if (low_objects[i].size && (low_objects[i].base>=LOW_LIMIT ||
            low_objects[i].size>LOW_LIMIT-low_objects[i].base))
            panic("BOOT OBJECT/FRAMEBUFFER MUST BE BELOW 4G");
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000u));
    if (a<0x80000001u) panic("PAGING NX REQUIRED");
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000001u));
    if (!(d&(1u<<20))) panic("PAGING NX REQUIRED");
    uint64_t cr4;
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    if (cr4&((1u<<12)|(1u<<17)|(1u<<21))) panic("PAGING LA57 PCID SMAP UNSUPPORTED");
    /* PCD+PWT selects PAT slot 3; require UC instead of assuming firmware PAT. */
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(0x277u));
    if ((a & 255)!=6 || (a>>24)!=0) panic("PAGING PAT UC SLOT REQUIRED");
    root=new_table();
    /* Only RAM descriptors are mapped (low identity / high direct-map). Device holes stay absent.
     * Runtime/ACPI memory is mapped NX but never allocated or invoked. */
    for (uint64_t off=0;off<boot->memory_map_bytes;off+=boot->descriptor_size) {
        const unsigned char *p=(const void *)(uintptr_t)(boot->memory_map+off);
        uint32_t type; uint64_t base,pages,attributes;
        __builtin_memcpy(&type,p,4); __builtin_memcpy(&base,p+8,8); __builtin_memcpy(&pages,p+24,8);
        __builtin_memcpy(&attributes,p+32,8);
        if (!((type>=1 && type<=7)||type==9||type==10||type==14) || base>=pmm_address_limit()) continue;
        uint64_t bytes=pages*PAGE;
        if (bytes>pmm_address_limit()-base) bytes=pmm_address_limit()-base;
        if (attributes & UINT64_C(0x2000)) continue; /* read-protected */
        uint64_t cache;
        if (attributes & 8) cache=0; /* WB capability */
        else if (attributes & 1) cache=24; /* UC capability */
        else continue; /* Unsupported cache policy: never guess WB. */
        uint64_t write=(attributes & UINT64_C(0x21000)) ? 0 : 2;
        map_range(base,bytes,write|NX|cache);
    }
    protect_range(boot->image.base,boot->image.size,2|NX);
    protect_range((uintptr_t)boot,sizeof(*boot),NX);
    protect_range(boot->map_buffer.base,boot->map_buffer.size,NX);
    protect_range(boot->user_image_buffer.base,boot->user_image_buffer.size,NX);
    protect_range((uintptr_t)__kernel_start,__kernel_end-__kernel_start,2|NX);
    protect_range((uintptr_t)__kernel_start,__text_end-__kernel_start,0);
    protect_range((uintptr_t)__rodata_start,__rodata_end-__rodata_start,NX);
    if (boot->framebuffer.valid) map_range(boot->framebuffer.base,boot->framebuffer.size,2|NX|24);
    require_flags((uintptr_t)paging_init,0);
    require_flags((uintptr_t)&root,2|NX);
    require_flags((uintptr_t)__rodata_start,NX);
    if (leaf(0,0) && *leaf(0,0)) panic("NULL PAGE MAPPED");
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(0xc0000080u));
    a|=1u<<11;
    __asm__ volatile("wrmsr"::"a"(a),"d"(d),"c"(0xc0000080u));
    cr4 &= ~(1u<<7); /* invalidate inherited global entries */
    __asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
    uint64_t cr0;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0)); cr0|=1u<<16;
    __asm__ volatile("mov %0,%%cr0"::"r"(cr0):"memory");
    __asm__ volatile("mov %0,%%cr3"::"r"(root):"memory");
    log_text("KRNL.EARLY_OWN_CR3=");log_hex(root);
    log_text("KRNL.PAGING_RX_RO_NX_NULL_GUARD tables=");log_hex(tables);
}
int paging_smp_prepare(uint64_t trampoline,uint64_t lapic) {
    if(!trampoline||trampoline>=0x100000||(trampoline&4095)||
       lapic>=LOW_LIMIT||(lapic&4095)||lapic<0x100000)return 0;
    uint64_t *code=leaf(trampoline,0),*device=leaf(lapic,0);
    if(!code||!(*code&1)||(*code&ADDR)!=trampoline||(*code&24)||
       (device&&(*device&1)))return 0;
    if(!paging_seal_code_page(trampoline))return 0;
    device=leaf(lapic,1);*device=lapic|NX|27; /* present/RW, PAT3 UC */
    *code=trampoline|1; /* read/execute; immutable after AP setup */
    __asm__ volatile("invlpg (%0);invlpg (%1)"::"r"(trampoline),"r"(lapic):"memory");
    return 1;
}

int paging_page_rw_wb(uint64_t physical) {
    if(!physical||(physical&4095)||!root)return 0;
    uint64_t cr3,cr4,pat;
#ifdef PAGING_HOST_TEST
    cr3=paging_test_cr3();cr4=paging_test_cr4();
#else
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
#endif
    if(cr3!=root||(cr4&((UINT64_C(1)<<12)|(UINT64_C(1)<<17))))return 0;
#ifdef PAGING_HOST_TEST
    pat=paging_test_pat();
#else
    uint32_t a,d;
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(0x277u));
    pat=((uint64_t)d<<32)|a;
#endif
    if((pat&255)!=6)return 0;
    uint64_t va=(uintptr_t)paging_phys_to_virt(physical),table=root;
    if(!va)return 0;
    int supervisor=0;
    for(int shift=39;shift>=12;shift-=9) {
        const uint64_t *view=paging_phys_to_virt(table);
        if(!view)return 0;
        uint64_t e=__atomic_load_n(&view[(va>>shift)&511],__ATOMIC_ACQUIRE);
        if((e&3)!=3||(e&24))return 0;
        /* Effective U/S is the AND of all levels: supervisor at any
         * level suffices, whereas RW must hold at every level. A split
         * table pointer may be user-accessible while its child leaf
         * remains supervisor-only. Do not reject that still-safe mapping. */
        if(!(e&4))supervisor=1;
        if(shift==12||(e&128)) {
            if(shift!=12&&shift!=21&&shift!=30)return 0;
            if(e&(shift==12?UINT64_C(128):UINT64_C(4096)))return 0;
            uint64_t mask=(UINT64_C(1)<<shift)-1;
            if((e&ADDR&mask)&~UINT64_C(4096))return 0;
            return supervisor && ((e&ADDR&~mask)|(va&mask))==physical;
        }
        table=e&ADDR;
        if(!table)return 0;
    }
    return 0;
}
