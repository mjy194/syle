#include "vmx_memory.h"
#define U64(x) UINT64_C(x)
static int valid_type(unsigned t) {
    return t==0 || t==1 || t==4 || t==5 || t==6;
}
int vmx_memory_snapshot_region_wb(const struct vmx_memory_snapshot *s,
                                  uint64_t physical) {
    if (!s || !s->cpu_mtrr || !s->cpu_pat ||
        s->physical_bits<32 || s->physical_bits>52 || (physical&4095)) return 0;
    uint64_t limit=U64(1)<<s->physical_bits;
    if (physical>=limit || (s->cr0 & (U64(3)<<29))) return 0; /* NW/CD */
    /* Paging uses slot 0 for all WB allocator RAM, never a guessed PAT type. */
    if ((s->pat&255)!=6 || !(s->def_type&(U64(1)<<11)) ||
        (s->def_type & ~U64(0xcff)) || !valid_type(s->def_type&255)) return 0;
    if ((s->def_type&(U64(1)<<10)) && !(s->cap&(U64(1)<<8))) return 0;
    uint64_t address_mask=(limit-1)&~U64(4095);
    unsigned matched=0;
    for (unsigned i=0;i<(s->cap&255);++i) {
        uint64_t b=s->variable[i].base, m=s->variable[i].mask;
        if (!(m&(U64(1)<<11))) continue;
        if ((b & ~(address_mask|255)) || (m & ~(address_mask|U64(0x800))) ||
            !valid_type(b&255)) return 0;
        uint64_t mask=m&address_mask;
        uint64_t size=((~mask)&(limit-1))+1;
        if (!size || (size&(size-1)) || ((b&address_mask)&(size-1))) return 0;
        if ((physical&mask)==(b&address_mask)) {
            /* Any matching non-WB type makes the result non-WB or undefined.
             * This also rejects WB/WT and WB/UC overlaps correctly. */
            if ((b&255)!=6) matched|=2;
            else matched|=1;
        }
    }
    if (physical<U64(0x100000) && (s->def_type&(U64(1)<<10))) {
        unsigned reg, byte;
        if (physical<U64(0x80000)) { reg=0; byte=physical>>16; }
        else if (physical<U64(0xc0000)) {
            reg=1+(unsigned)((physical-U64(0x80000))>>17);
            byte=(physical>>14)&7;
        } else {
            reg=3+(unsigned)((physical-U64(0xc0000))>>15);
            byte=(physical>>12)&7;
        }
        return ((s->fixed[reg]>>(byte*8))&255)==6;
    }
    return matched ? matched==1 : (s->def_type&255)==6;
}
#ifndef VMX_MEMORY_POLICY_ONLY
static uint64_t read_msr(uint32_t n) {
    uint32_t a,d;
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));
    return ((uint64_t)d<<32)|a;
}
int vmx_memory_region_wb(uint64_t physical) {
    /* Bootstrap-only, one CPU and interrupts disabled. Snapshot is private to
     * this call; no hidden static cache can survive a later MTRR change. */
    struct vmx_memory_snapshot s={0};
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1u),"c"(0u));
    s.cpu_mtrr=!!(d&(1u<<12)); s.cpu_pat=!!(d&(1u<<16));
    if (!s.cpu_mtrr || !s.cpu_pat) return 0;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000u),"c"(0u));
    if (a<0x80000008u) return 0;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000008u),"c"(0u));
    s.physical_bits=a&255;
    __asm__ volatile("mov %%cr0,%0":"=r"(s.cr0));
    s.cap=read_msr(0xfe); s.def_type=read_msr(0x2ff); s.pat=read_msr(0x277);
    for (unsigned i=0;i<(s.cap&255);++i) {
        s.variable[i].base=read_msr(0x200+i*2);
        s.variable[i].mask=read_msr(0x201+i*2);
    }
    if ((s.cap&(U64(1)<<8)) && (s.def_type&(U64(1)<<10))) {
        s.fixed[0]=read_msr(0x250); s.fixed[1]=read_msr(0x258); s.fixed[2]=read_msr(0x259);
        for (unsigned i=0;i<8;++i) s.fixed[i+3]=read_msr(0x268+i);
    }
    return vmx_memory_snapshot_region_wb(&s,physical);
}
#endif
