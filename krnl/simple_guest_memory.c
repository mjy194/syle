#include "simple_guest.h"
#define PHYSICAL_MASK UINT64_C(0x000ffffffffff000)
#define NX (UINT64_C(1)<<63)
static void zero(void *p) {
    unsigned char *b=p;
    for (unsigned i=0;i<SG_PAGE_SIZE;++i) b[i]=0;
}
bool sg_memory_release(struct sg_memory *m, const struct sg_memory_ops *ops) {
    if (!m || !ops || !ops->release) return false;
    bool ok=true;
    m->prepared=false;
    m->eptp=0;
    for (unsigned i=0;i<SG_PAGE_COUNT;++i) if (m->pages[i]) {
        if (ops->release(ops->context,m->pages[i])) m->pages[i]=0;
        else ok=false;
    }
    for (unsigned i=0;i<4;++i) if (m->tables[i]) {
        if (ops->release(ops->context,m->tables[i])) m->tables[i]=0;
        else ok=false;
    }
    if (ok) *m=(struct sg_memory){0};
    return ok;
}
static void *page(uint64_t *owned, const struct sg_memory_ops *ops) {
    *owned=ops->allocate(ops->context);
    if (!*owned || (*owned & ~PHYSICAL_MASK) || !ops->writeback(ops->context,*owned)) return 0;
    void *p=ops->map(ops->context,*owned);
    if (!p || ((uintptr_t)p & (sizeof(uint64_t)-1))) return 0;
    zero(p);
    return p;
}
bool sg_memory_prepare(struct sg_memory *m, const struct sg_memory_ops *ops, const void *bytes, size_t size) {
    struct sg_image im;
    if (!m || !ops || !ops->allocate || !ops->release || !ops->map || !ops->writeback) return false;
    if (m->prepared || m->eptp) return false;
    for (unsigned i=0;i<SG_PAGE_COUNT;++i) if (m->pages[i] || m->permissions[i]) return false;
    for (unsigned i=0;i<4;++i) if (m->tables[i]) return false;
    if (!sg_image_inspect(bytes,size,&im)) return false;
    unsigned char *ram[SG_PAGE_COUNT]={0};
    uint64_t *ept[4]={0};
    for (unsigned i=1;i<SG_PAGE_COUNT;++i) {
        ram[i]=page(&m->pages[i],ops);
        if (!ram[i]) goto fail;
        m->permissions[i]=3;
    }
    for (unsigned i=0;i<4;++i) {
        ept[i]=page(&m->tables[i],ops);
        if (!ept[i]) goto fail;
    }
    m->permissions[SG_BOOT_INFO/SG_PAGE_SIZE]=1;
    for (unsigned i=0;i<im.count;++i) {
        const struct sg_segment *s=&im.segments[i];
        for (uint64_t p=s->address/SG_PAGE_SIZE;p<=(s->address+s->memsz-1)/SG_PAGE_SIZE;++p)
            m->permissions[p]=(s->flags==5?5:s->flags==6?3:1);
        for (uint64_t n=0;n<s->filesz;++n) {
            uint64_t a=s->address+n;
            ram[a/SG_PAGE_SIZE][a%SG_PAGE_SIZE]=((const unsigned char *)bytes)[s->offset+n];
        }
    }
    /* Guest identity mapping, supervisor pages only; null is intentionally absent. */
    ((uint64_t *)ram[1])[0]=UINT64_C(0x2003);
    ((uint64_t *)ram[2])[0]=UINT64_C(0x3003);
    ((uint64_t *)ram[3])[0]=UINT64_C(0x4003);
    for (unsigned i=1;i<SG_PAGE_COUNT;++i)
        ((uint64_t *)ram[4])[i]=(uint64_t)i*SG_PAGE_SIZE | 1 |
            ((m->permissions[i]&2)?2:0) | ((m->permissions[i]&4)?0:NX);
    uint64_t *gdt=(uint64_t *)ram[5];
    gdt[1]=UINT64_C(0x00af9b000000ffff);
    gdt[2]=UINT64_C(0x00cf93000000ffff);
    gdt[3]=UINT64_C(0x00008b0000000067) | (SG_TSS << 16);
    gdt[4]=0;
    /* TSS I/O bitmap beyond limit: guest has no I/O bitmap permissions. */
    ram[5][0x100+102]=104;
    struct sg_boot_record *boot=(struct sg_boot_record *)ram[6];
    *boot=(struct sg_boot_record){SG_BOOT_MAGIC,SG_BOOT_VERSION,sizeof(*boot),SG_RAM_SIZE,SG_STACK_TOP,im.entry};
    for (unsigned i=0;i<3;++i) ept[i][0]=m->tables[i+1]|7;
    for (unsigned i=1;i<SG_PAGE_COUNT;++i) ept[3][i]=m->pages[i]|UINT64_C(0x30)|m->permissions[i];
    m->eptp=m->tables[0]|UINT64_C(0x1e); /* WB, four levels, no A/D. */
    m->image=im;
    m->prepared=true;
    return true;
fail:
    (void)sg_memory_release(m,ops);
    return false;
}
