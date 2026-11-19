static uint32_t cpu_id(void) {
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    return b>>24;
}
static uint64_t msr(unsigned n){uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return ((uint64_t)d<<32)|a;}
static uint8_t in8(uint16_t p){uint8_t x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(p));return x;}
static void out8(uint16_t p,uint8_t x){__asm__ volatile("outb %0,%1"::"a"(x),"Nd"(p));}
/* PIT channel2 one-shot, 1.193182 MHz. No guessed host TSC frequency.
 * Bounded poll count makes a broken timer a diagnostic failure. */
static bool delay(unsigned ticks) {
    uint8_t old=in8(0x61);out8(0x61,old&~3u);
    out8(0x43,0xb0);out8(0x42,ticks&255);out8(0x42,ticks>>8);
    out8(0x61,(old&~2u)|1u);
    bool done=false;
    for(unsigned i=0;i<10000000;i++) {
        if(in8(0x61)&0x20){done=true;break;}
        __asm__ volatile("pause");
    }
    out8(0x61,old);return done;
}
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
static bool low_owned(const struct syle_boot_info *b) {
    uint64_t p=b->ap_trampoline.base;
    if(!p||p>=0x100000||(p&4095)||b->ap_trampoline.size!=4096)return false;
    for(uint64_t off=0;off<b->memory_map_bytes;off+=b->descriptor_size) {
        const unsigned char *d=(void *)(uintptr_t)(b->memory_map+off);
        uint32_t type;uint64_t base,pages,attr;
        __builtin_memcpy(&type,d,4);__builtin_memcpy(&base,d+8,8);
        __builtin_memcpy(&pages,d+24,8);__builtin_memcpy(&attr,d+32,8);
        if(type==2&&(attr&8)&&!(attr&0x21000)&&pages<=UINT64_MAX/4096&&
           base<=UINT64_MAX-pages*4096&&p>=base&&p+4096<=base+pages*4096)return true;
    }
    return false;
}
static bool icr(volatile uint32_t *lapic,uint32_t target,uint32_t command) {
    unsigned i=0;for(;i<10000000&&(lapic[0x300/4]&(1u<<12));i++)__asm__ volatile("pause");
    if(i==10000000)return false;
    lapic[0x310/4]=target<<24;lapic[0x300/4]=command;
    for(i=0;i<10000000&&(lapic[0x300/4]&(1u<<12));i++)__asm__ volatile("pause");
    return i<10000000;
}
