#include "smp_ept.h"
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#include "../efi/src/exceptions.h"
extern const unsigned char smp_guest_payload_start[],smp_guest_payload_end[];
static uint64_t *view(uint64_t hpa){return paging_phys_to_virt(hpa);}
static uint64_t alloc(void) {
    uint64_t p=pmm_alloc_page_below(UINT64_C(0x100000000));
    if(!p)return 0;
    if(!vmx_memory_region_wb(p)){pmm_free_page(p);return 0;}
    uint64_t *v=view(p);if(!v){if(!pmm_free_page(p))panic("SMP EPT UNMAPPED FREE");return 0;}for(unsigned i=0;i<512;i++)v[i]=0;return p;
}
void smp_ept_release(struct smp_ept *f) {
    for(unsigned i=0;i<4;i++)if(f->table[i]){if(!pmm_free_page(f->table[i]))panic("SMP EPT FREE");f->table[i]=0;}
    for(unsigned i=0;i<10;i++)if(f->page[i]){if(!pmm_free_page(f->page[i]))panic("SMP GUEST FREE");f->page[i]=0;}
    if(f->replacement_page){if(!pmm_free_page(f->replacement_page))panic("SMP EPT REPLACEMENT FREE");f->replacement_page=0;}
    f->eptp=0;
}
bool smp_ept_prepare(struct smp_ept *f) {
    *f=(struct smp_ept){0};
    unsigned size=smp_guest_payload_end-smp_guest_payload_start;
    if(!size||size>4096)return false;
    for(unsigned i=0;i<4;i++)if(!(f->table[i]=alloc()))goto fail;
    for(unsigned i=0;i<10;i++)if(!(f->page[i]=alloc()))goto fail;
    if(!(f->replacement_page=alloc()))goto fail;
    *view(f->replacement_page)=UINT64_C(0x8877665544332211);
    for(unsigned i=0;i<3;i++)view(f->table[i])[0]=f->table[i+1]|7;
    for(unsigned i=1;i<=10;i++) {
        uint64_t perms=i==6?5:(i>=9?3:1);
        view(f->table[3])[i]=f->page[i-1]|(6u<<3)|perms;
    }
    for(unsigned i=0;i<3;i++)view(f->page[i])[0]=((i+2)*4096)|0x63;
    for(unsigned i=1;i<=10;i++)view(f->page[3])[i]=(i*4096)|0x63;
    uint64_t *g=view(f->page[4]);g[1]=UINT64_C(0x00af9b000000ffff);g[2]=UINT64_C(0x00cf93000000ffff);
    g[3]=103|(UINT64_C(0x5100)<<16)|(UINT64_C(0x89)<<40);g[4]=0;
    unsigned char *code=(void *)view(f->page[5]);for(unsigned i=0;i<size;i++)code[i]=smp_guest_payload_start[i];
    *view(f->page[7])=UINT64_C(0x1122334455667788);
    f->eptp=f->table[0]|0x1e;
    __atomic_thread_fence(__ATOMIC_RELEASE);return true;
fail:smp_ept_release(f);return false;
}
uint64_t smp_ept_stack_word(const struct smp_ept *f,unsigned cpu) {
    return cpu<2?view(f->page[8+cpu])[511]:0;
}

bool smp_ept_replace_data(struct smp_ept *f) {
    /* No allocator, invalidation, reclamation or guest execution here. */
    if(!f || !f->eptp || f->eptp!=(f->table[0]|0x1e) ||
       !f->table[3] || (f->table[3]&4095) || !f->page[7] ||
       (f->page[7]&4095) || !f->replacement_page ||
       (f->replacement_page&4095) || f->replacement_page>=UINT64_C(0x100000000) ||
       f->replacement_page==f->page[7])return false;
    for(unsigned i=0;i<4;i++)if(f->replacement_page==f->table[i])return false;
    for(unsigned i=0;i<10;i++)if(f->replacement_page==f->page[i])return false;
    uint64_t *pt=view(f->table[3]);
    if(!pt || ((uintptr_t)pt&4095) || !view(f->replacement_page))return false;
    const uint64_t flags=(6u<<3)|1u; /* WB, readable, neither writable nor executable. */
    if(__atomic_load_n(&pt[8],__ATOMIC_ACQUIRE)!=(f->page[7]|flags))return false;
    /* x86 aligned 64-bit publication; release orders initialized backing.
     * NOT a replacement for the caller's per-CPU INVEPT acknowledgement. */
    __atomic_store_n(&pt[8],f->replacement_page|flags,__ATOMIC_RELEASE);
    return true;
}

/* Repeatable bounded mapping selection; both WB backing pages remain owned. */
bool smp_ept_select_data(struct smp_ept *f,unsigned index) {
    if(!f||index>1||!f->table[3]||!f->page[7]||!f->replacement_page)return false;
    uint64_t *pt=view(f->table[3]);if(!pt)return false;
    uint64_t old=__atomic_load_n(&pt[8],__ATOMIC_ACQUIRE);
    if(old!=(f->page[7]|0x31)&&old!=(f->replacement_page|0x31))return false;
    uint64_t target=index?f->replacement_page:f->page[7];
    if(target&4095)return false;
    __atomic_store_n(&pt[8],target|0x31,__ATOMIC_RELEASE);return true;
}

extern const unsigned char smp_guest_running_payload_start[],smp_guest_running_payload_end[];
bool smp_ept_prepare_running(struct smp_ept *f) {
    if(!f)return false;
    uintptr_t start=(uintptr_t)smp_guest_running_payload_start;
    uintptr_t end=(uintptr_t)smp_guest_running_payload_end;
    if(end<=start || end-start>4096)return false;
    if(!smp_ept_prepare(f))return false;
    uint64_t *pt=view(f->table[3]),*comm=view(f->page[6]);
    unsigned char *code=(void *)view(f->page[5]);
    if(!pt || !comm || !code || ((uintptr_t)pt&4095) ||
       ((uintptr_t)comm&4095) || ((uintptr_t)code&4095) ||
       pt[7]!=(f->page[6]|0x31) || pt[6]!=(f->page[5]|0x35)){
        smp_ept_release(f);return false;
    }
    /* Preparation remains BSP-private; prepare's release fence by itself
     * does not expose f to APs. Caller publishes only after this returns. */
    for(unsigned i=0;i<4096;i++)code[i]=0;
    for(uintptr_t i=0;i<end-start;i++)code[i]=smp_guest_running_payload_start[i];
    for(unsigned i=0;i<512;i++)comm[i]=0;
    pt[7]=f->page[6]|0x33; /* RW WB, NX communication page. */
    __atomic_thread_fence(__ATOMIC_RELEASE);
    return true;
}
static uint64_t *communication(const struct smp_ept *f) {
    if(!f || !f->eptp || !f->table[3] || (f->table[3]&4095) ||
       !f->page[6] || (f->page[6]&4095))return 0;
    uint64_t *pt=view(f->table[3]),*comm=view(f->page[6]);
    if(!pt || !comm || ((uintptr_t)pt&4095) || ((uintptr_t)comm&4095) ||
       __atomic_load_n(&pt[7],__ATOMIC_ACQUIRE)!=(f->page[6]|0x33))return 0;
    return comm;
}
bool smp_ept_heartbeat(const struct smp_ept *f,uint64_t *old) {
    if(!old)return false;
    uint64_t *comm=communication(f);
    if(!comm || __atomic_load_n(&comm[1],__ATOMIC_ACQUIRE)!=1)return false;
    *old=__atomic_load_n(&comm[0],__ATOMIC_RELAXED);
    return true;
}
bool smp_ept_stop(struct smp_ept *f) {
    uint64_t *comm=communication(f);
    if(!comm)return false;
    __atomic_store_n(&comm[2],UINT64_C(1),__ATOMIC_RELEASE);
    return true;
}
