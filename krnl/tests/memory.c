#include "../memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct efi_desc { uint32_t type, pad; uint64_t base, virt, pages, attr; };
static struct efi_desc map[12];
static struct syle_boot_info b;
static void setup(void) {
    memset(&b,0,sizeof(b)); memset(map,0,sizeof(map));
    b.magic=SYLE_BOOT_MAGIC; b.version=SYLE_BOOT_VERSION; b.size=sizeof(b);
    b.boot_services_exited=1; b.descriptor_size=sizeof(map[0]); b.descriptor_version=1;
    b.memory_map=(uintptr_t)map; b.memory_map_bytes=sizeof(map);
    b.map_buffer=(struct syle_range){(uintptr_t)map,sizeof(map)};
    map[0]=(struct efi_desc){7,0,0x100000,0,16,8};
    map[1]=(struct efi_desc){4,0,0x200000,0,8,8}; /* firmware never reclaimed */
    map[2]=(struct efi_desc){7,0,0x300000,0,8,(UINT64_C(1)<<63)|8};
    map[3]=(struct efi_desc){7,0,0xffffe000,0,4,8}; /* crosses 4GiB */
    map[4]=(struct efi_desc){7,0,0x400000,0,8,8};
    map[5]=(struct efi_desc){7,0,0x500000,0,8,8};
    map[6]=(struct efi_desc){7,0,0x600000,0,8,8};
    map[7]=(struct efi_desc){7,0,0x80000,0,16,8}; /* low memory excluded */
    map[8]=(struct efi_desc){7,0,0x700000,0,8,0x1008};
    map[9]=(struct efi_desc){7,0,0x800000,0,8,0x2008};
    map[10]=(struct efi_desc){7,0,0x900000,0,8,0x20008};
    map[11]=(struct efi_desc){7,0,0xa00000,0,8,1}; /* UC-only: not allocatable */
    b.acpi_rsdp=0x400042; b.inherited_rsp=0x500124; b.inherited_cr3=0x600000;
    b.image=(struct syle_range){0x102001,1};
    b.kernel_image=(struct syle_range){0x104000,4096};
    b.user_image=(struct syle_range){0x106001,31};
    b.user_image_buffer=(struct syle_range){0x106000,8192};
    b.framebuffer.base=0x108fff; b.framebuffer.size=2; /* touches two pages */
}
static void fail_closed(void) {
    assert(!pmm_init(&b)); assert(!pmm_alloc_page());
    struct pmm_stats s=pmm_get_stats();
    assert(!s.usable_pages && !s.free_pages && !s.allocated_pages);
}
int main(int argc, char **argv) {
    if (argc>1 && (!strcmp(argv[1],"zero") || !strcmp(argv[1],"single"))) {
        setup(); b.memory_map_bytes=sizeof(map[0]);
        map[0]=(struct efi_desc){7,0,0x100000,0,1,8};
        if (!strcmp(argv[1],"zero")) b.kernel_image=(struct syle_range){0x100000,4096};
        assert(pmm_init(&b));
        uint64_t p=pmm_alloc_page();
        if (!strcmp(argv[1],"zero")) {
            assert(!p && !pmm_get_stats().usable_pages);
            assert(!pmm_free_page(0x100000));
        } else {
            assert(p==0x100000 && !pmm_alloc_page());
            assert(pmm_free_page(p) && pmm_alloc_page()==p);
            assert(pmm_get_stats().allocated_pages==1 && !pmm_get_stats().free_pages);
        }
        puts("PMM.ZERO_SINGLE_BOUNDARY_PASS"); return 0;
    }
    if (argc>1 && !strcmp(argv[1],"cap")) {
        setup(); b.memory_map_bytes=sizeof(map[0]);
        map[0]=(struct efi_desc){7,0,PMM_DIRECT_MAP_LIMIT-4096,0,2,8};
        assert(pmm_init(&b));
        assert(pmm_address_limit()==PMM_DIRECT_MAP_LIMIT);
        assert(pmm_get_stats().usable_pages==1);
        assert(pmm_alloc_page()==PMM_DIRECT_MAP_LIMIT-4096);
        assert(!pmm_alloc_page());
        assert(pmm_free_page(PMM_DIRECT_MAP_LIMIT-4096));
        assert(!pmm_free_page(PMM_DIRECT_MAP_LIMIT));
        puts("PMM.CANONICAL_CAP_TEST_PASS"); return 0;
    }
    if (argc>1 && !strcmp(argv[1],"cpu")) {
        setup(); fail_closed(); puts("PMM.INVALID_CPU_TEST_PASS"); return 0;
    }
    if (argc>1 && !strcmp(argv[1],"sparse")) {
        setup(); b.memory_map_bytes=2*sizeof(map[0]);
        map[0]=(struct efi_desc){7,0,UINT64_C(1)<<40,0,3,8};
        map[1]=(struct efi_desc){7,0,UINT64_C(1)<<45,0,2,8};
        assert(pmm_init(&b)); assert(pmm_get_stats().usable_pages==5);
        assert(pmm_alloc_page_above(UINT64_C(1)<<45)==(UINT64_C(1)<<45));
        assert(pmm_alloc_page()==(UINT64_C(1)<<40));
        assert(!pmm_alloc_page_below(UINT64_C(1)<<40));
        assert(!pmm_free_page((UINT64_C(1)<<40)+0x10000));
        puts("PMM.SPARSE_TEST_PASS"); return 0;
    }
    if (argc>1) {
        setup();
        b.memory_map_bytes=sizeof(map[0]);
        map[0]=(struct efi_desc){7,0,UINT64_C(0x100000000),0,PMM_MAX_RANGES*2+2,8};
        assert(pmm_init(&b));
        uint64_t base=UINT64_C(0x100000000);
        for (unsigned i=0;i<PMM_MAX_RANGES*2+2;++i) assert(pmm_alloc_page()==base+i*4096);
        for (unsigned i=0;i<PMM_MAX_RANGES;++i) assert(pmm_free_page(base+i*8192));
        struct pmm_stats before=pmm_get_stats();
        assert(!pmm_free_page(base+PMM_MAX_RANGES*8192));
        assert(pmm_get_stats().free_pages==before.free_pages);
        assert(pmm_free_page(base+4096)); /* merge two extents */
        assert(pmm_free_page(base+PMM_MAX_RANGES*8192));
        assert(!pmm_free_page(base+PMM_MAX_RANGES*8192));
        puts("PMM.FRAGMENTATION_TEST_PASS"); return 0;
    }
    assert(!pmm_init(NULL)); assert(!pmm_free_page(0x100000));
    setup(); b.memory_map_bytes=(PMM_MAX_DESCRIPTORS+1)*sizeof(map[0]);
    b.map_buffer.size=b.memory_map_bytes; fail_closed(); /* rejected before descriptor dereference */
    setup(); b.descriptor_version=2; fail_closed();
    setup(); b.memory_map_bytes--; fail_closed();
    setup(); b.descriptor_size=39; fail_closed();
    setup(); b.map_buffer.size--; fail_closed();
    setup(); b.memory_map=UINT64_MAX-16; b.map_buffer.base=b.memory_map; fail_closed();
    setup(); b.kernel_image=(struct syle_range){UINT64_MAX,2}; fail_closed();
    setup(); map[0].pages=0; fail_closed();
    setup(); map[0].base++; fail_closed();
    setup(); map[0].pages=UINT64_MAX; fail_closed();
    setup(); map[1].base=0x100000; fail_closed();
    setup(); map[1].base=0x10f000; fail_closed();
    setup(); b.reclaim_allowed=1; fail_closed();
    setup(); map[3].base=(UINT64_C(1)<<PMM_TEST_PHYS_BITS)-4096;
    fail_closed(); /* valid arithmetic, but RAM crosses CPU physical width */
    setup(); assert(pmm_init(&b));
    assert(pmm_address_limit()==(UINT64_C(1)<<PMM_TEST_PHYS_BITS));
    assert(!pmm_alloc_page_above(UINT64_MAX));
    assert(!pmm_alloc_page_above(pmm_address_limit()));
    assert(!pmm_alloc_page_below(0x100000));
    uint64_t high=pmm_alloc_page_above(UINT64_C(0x100000000));
    assert(high==UINT64_C(0x100000000));
    assert(pmm_free_page(high));
    high=pmm_alloc_page_above(UINT64_C(0x100000001));
    assert(high==UINT64_C(0x100001000));
    assert(pmm_free_page(high));
    uint64_t low=pmm_alloc_page_below(0x101001);
    assert(low==0x100000); assert(pmm_free_page(low));
    struct pmm_stats s=pmm_get_stats(); assert(s.usable_pages==14 && s.free_pages==14);
    uint64_t pages[14];
    for (unsigned i=0; i<14; ++i) {
        pages[i]=pmm_alloc_page(); assert(pages[i]);
        assert(pages[i]%4096==0 && pages[i]<pmm_address_limit());
        assert((pages[i]>=0x100000 && pages[i]<0x110000) || pages[i]>=0xffffe000);
        assert(pages[i]!=0x102000 && pages[i]!=0x104000 && pages[i]!=0x106000 &&
               pages[i]!=0x107000 && pages[i]!=0x108000 && pages[i]!=0x109000);
        for (unsigned j=0; j<i; ++j) assert(pages[j]!=pages[i]);
    }
    assert(!pmm_alloc_page()); assert(!pmm_init(&b));
    s=pmm_get_stats(); assert(s.allocated_pages==14 && !s.free_pages);
    assert(!pmm_free_page(0)); assert(!pmm_free_page(pmm_address_limit()));
    assert(!pmm_free_page(pages[0]+1)); assert(!pmm_free_page(0x102000));
    assert(!pmm_free_page(0x200000));
    assert(pmm_free_page(pages[0])); assert(!pmm_free_page(pages[0]));
    assert(pmm_alloc_page()==pages[0]);
    for (unsigned i=0;i<14;++i) assert(pmm_free_page(pages[i]));
    s=pmm_get_stats(); assert(s.free_pages==14 && !s.allocated_pages);
    puts("PMM.TESTS_PASS");
}
