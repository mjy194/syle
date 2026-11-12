#include "memory.h"
#include <stddef.h>
#define EFI_CONVENTIONAL 7u
#define EFI_WB UINT64_C(8)
#define EFI_RUNTIME (UINT64_C(1) << 63)
#define EFI_PROTECTED (UINT64_C(0x1000) | UINT64_C(0x2000) | UINT64_C(0x20000))
struct extent { uint64_t start, end; }; /* page-aligned, exclusive end */
static struct extent eligible[PMM_MAX_RANGES], available[PMM_MAX_RANGES];
static unsigned eligible_count, available_count;
static uint64_t address_limit;
static uint64_t cpu_limit(void) {
#ifdef PMM_TEST_PHYS_BITS
    unsigned bits = PMM_TEST_PHYS_BITS;
#else
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000u),"c"(0));
    if (a < 0x80000008u) return 0;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000008u),"c"(0));
    unsigned bits = a & 255u;
#endif
    if (bits < 32 || bits > 52) return 0;
    return UINT64_C(1) << bits;
}
uint64_t pmm_address_limit(void) { return address_limit; }
/* Sorted insertion/coalescing; capacity failure never changes the list. */
static int insert(struct extent *list, unsigned *count, uint64_t start, uint64_t end) {
    unsigned i=0;
    while (i<*count && list[i].end<start) ++i;
    unsigned j=i;
    while (j<*count && list[j].start<=end) {
        if (list[j].start<start) start=list[j].start;
        if (list[j].end>end) end=list[j].end;
        ++j;
    }
    if (i==j && *count==PMM_MAX_RANGES) return 0;
    if (i==j) {
        for (unsigned k=*count;k>i;--k) list[k]=list[k-1];
        ++*count;
    } else {
        for (unsigned k=j;k<*count;++k) list[i+1+k-j]=list[k];
        *count-=j-i-1;
    }
    list[i]=(struct extent){start,end};
    return 1;
}
static void remove_at(struct extent *list, unsigned *count, unsigned i) {
    for (unsigned j=i+1;j<*count;++j) list[j-1]=list[j];
    --*count;
}
static struct pmm_stats stats;
static int ready;
struct descriptor { uint32_t type; uint64_t base, pages, attributes; };
static uint64_t read64(const unsigned char *p) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (i*8);
    return v;
}
static struct descriptor descriptor_at(const struct syle_boot_info *b, uint64_t i) {
    const unsigned char *p = (const unsigned char *)(uintptr_t)b->memory_map + i*b->descriptor_size;
    struct descriptor d = {0};
    for (unsigned j = 0; j < 4; ++j) d.type |= (uint32_t)p[j] << (j*8);
    d.base = read64(p+8); d.pages = read64(p+24); d.attributes = read64(p+32);
    return d;
}
static int range_valid(struct syle_range r) { return r.size <= UINT64_MAX-r.base; }
static int reserve(struct syle_range r) {
    if (!r.size || r.base >= address_limit) return 1;
    uint64_t end=r.base+r.size;
    if (end>address_limit) end=address_limit;
    uint64_t start=r.base&~(PMM_PAGE_SIZE-1);
    end=(end+PMM_PAGE_SIZE-1)&~(PMM_PAGE_SIZE-1);
    for (unsigned i=0;i<eligible_count;) {
        struct extent e=eligible[i];
        if (start>=e.end || end<=e.start) { ++i; continue; }
        if (start<=e.start && end>=e.end) { remove_at(eligible,&eligible_count,i); continue; }
        if (start<=e.start) eligible[i].start=end;
        else if (end>=e.end) eligible[i].end=start;
        else {
            if (eligible_count==PMM_MAX_RANGES) return 0;
            for (unsigned j=eligible_count;j>i+1;--j) eligible[j]=eligible[j-1];
            eligible[i].end=start;
            eligible[i+1]=(struct extent){end,e.end};
            ++eligible_count;
        }
        ++i;
    }
    return 1;
}
int pmm_init(const struct syle_boot_info *b) {
    /* Never erase ownership for an already initialized allocator. */
    if (ready) return 0;
    eligible_count=available_count=0;
    stats = (struct pmm_stats){0};
    uint64_t physical_limit=cpu_limit();
    address_limit=physical_limit<PMM_DIRECT_MAP_LIMIT ? physical_limit : PMM_DIRECT_MAP_LIMIT;
    if (!physical_limit) return 0;
    if (!b || b->magic != SYLE_BOOT_MAGIC || b->version != SYLE_BOOT_VERSION ||
        b->size != sizeof(*b) || b->boot_services_exited != 1 || b->reclaim_allowed ||
        !b->memory_map || !b->memory_map_bytes || b->descriptor_size < 40 ||
        b->descriptor_size % 8 || b->descriptor_version != 1 ||
        b->memory_map_bytes % b->descriptor_size ||
        b->memory_map_bytes > UINT64_MAX-b->memory_map ||
        b->map_buffer.base != b->memory_map || b->memory_map_bytes > b->map_buffer.size)
        return 0;
    uint64_t count=b->memory_map_bytes/b->descriptor_size;
    if (count > PMM_MAX_DESCRIPTORS) return 0;
    struct syle_range ranges[] = {
        {0, UINT64_C(0x100000)}, b->image, b->kernel_image,
        b->user_image, b->user_image_buffer, b->map_buffer,
        {(uintptr_t)b,sizeof(*b)}, {b->framebuffer.base,b->framebuffer.size}
    };
    for (unsigned i=0; i<sizeof(ranges)/sizeof(ranges[0]); ++i)
        if (!range_valid(ranges[i])) return 0;
    for (uint64_t i=0; i<count; ++i) {
        struct descriptor a=descriptor_at(b,i);
        if (a.base%PMM_PAGE_SIZE || !a.pages || a.pages > (UINT64_MAX-a.base)/PMM_PAGE_SIZE)
            return 0;
        uint64_t end=a.base+a.pages*PMM_PAGE_SIZE;
        if (((a.type>=1 && a.type<=7) || a.type==9 || a.type==10 || a.type==14) &&
            end>physical_limit) return 0;
        for (uint64_t j=0; j<i; ++j) {
            struct descriptor d=descriptor_at(b,j);
            if (a.base < d.base+d.pages*PMM_PAGE_SIZE && d.base < end) return 0;
        }
    }
    /* Live state is published only after all ranges and reservations succeed. */
    for (uint64_t i=0; i<count; ++i) {
        struct descriptor d=descriptor_at(b,i);
        uint64_t end=d.base+d.pages*PMM_PAGE_SIZE;
        if (d.type != EFI_CONVENTIONAL || !(d.attributes & EFI_WB) ||
            (d.attributes & (EFI_RUNTIME | EFI_PROTECTED)) || d.base >= address_limit) continue;
        /* Keep entire descriptors containing opaque inherited objects. Their
         * sizes are not known well enough to reclaim a neighboring page. */
        if ((b->acpi_rsdp >= d.base && b->acpi_rsdp < end) ||
            (b->inherited_rsp >= d.base && b->inherited_rsp < end) ||
            ((b->inherited_cr3 & ~UINT64_C(4095)) >= d.base &&
             (b->inherited_cr3 & ~UINT64_C(4095)) < end)) continue;
        if (end > address_limit) end=address_limit;
        if (!insert(eligible,&eligible_count,d.base,end)) return 0;
    }
    for (unsigned i=0; i<sizeof(ranges)/sizeof(ranges[0]); ++i)
        if (!reserve(ranges[i])) return 0;
    for (unsigned i=0; i<eligible_count; ++i) {
        available[i]=eligible[i];
        stats.usable_pages+=(eligible[i].end-eligible[i].start)/PMM_PAGE_SIZE;
    }
    available_count=eligible_count;
    stats.free_pages=stats.usable_pages; ready=1;
    return 1;
}
static uint64_t allocate_between(uint64_t min, uint64_t max) {
    if (!ready || min>=max || min>=address_limit) return 0;
    if (max>address_limit) max=address_limit;
    min=(min+PMM_PAGE_SIZE-1)&~(PMM_PAGE_SIZE-1);
    max&=~(PMM_PAGE_SIZE-1);
    for (unsigned i=0;i<available_count;++i) {
        struct extent e=available[i];
        uint64_t p=e.start>min ? e.start : min;
        if (p>=e.end || p>=max) continue;
        uint64_t end=p+PMM_PAGE_SIZE;
        if (p>e.start && end<e.end) {
            if (available_count==PMM_MAX_RANGES) return 0;
            for (unsigned j=available_count;j>i+1;--j) available[j]=available[j-1];
            available[i].end=p;
            available[i+1]=(struct extent){end,e.end};
            ++available_count;
        } else if (p==e.start && end==e.end) remove_at(available,&available_count,i);
        else if (p==e.start) available[i].start=end;
        else available[i].end=p;
        --stats.free_pages; ++stats.allocated_pages;
        return p;
    }
    return 0;
}
uint64_t pmm_alloc_page(void) { return allocate_between(0,address_limit); }
uint64_t pmm_alloc_page_below(uint64_t limit) { return allocate_between(0,limit); }
uint64_t pmm_alloc_page_above(uint64_t min) { return allocate_between(min,address_limit); }
int pmm_free_page(uint64_t address) {
    if (!ready || !address || address%PMM_PAGE_SIZE || address>=address_limit) return 0;
    int admitted=0;
    for (unsigned i=0;i<eligible_count;++i)
        if (address>=eligible[i].start && address<eligible[i].end) { admitted=1; break; }
    if (!admitted) return 0;
    for (unsigned i=0;i<available_count;++i)
        if (address>=available[i].start && address<available[i].end) return 0;
    /* Eligible and not free means allocated. If fragmentation fills metadata,
     * refuse the free unchanged; the caller still owns that page and may retry. */
    if (!insert(available,&available_count,address,address+PMM_PAGE_SIZE)) return 0;
    ++stats.free_pages; --stats.allocated_pages;
    return 1;
}
struct pmm_stats pmm_get_stats(void) { return stats; }
