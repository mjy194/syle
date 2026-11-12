/* Public PMM API model test. Synthetic physical addresses are never
 * dereferenced. The bitmap oracle does not use the production extent code. */
#include "../memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE UINT64_C(4096)
#define LOW UINT64_C(0x200000)
#define CROSS UINT64_C(0xffff0000)
#define HIGH1 (UINT64_C(1) << 40)
#define HIGH32 (UINT64_C(1) << 45)
#define MODEL_PAGES 192u
struct descriptor { uint32_t type, pad; uint64_t base, virt, pages, attr; };
static struct descriptor map[15];
static struct syle_boot_info boot;
struct page { uint64_t address; int eligible, allocated; };
static struct page model[MODEL_PAGES];
static unsigned used, calls;
static uint32_t random_state;
static uint32_t next_random(void) {
    uint32_t x = random_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return random_state = x;
}
static void add_pages(uint64_t base, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        assert(used < MODEL_PAGES);
        model[used++] = (struct page){base + i * PAGE, 1, 0};
    }
}
static void boot_header(void) {
    memset(&boot, 0, sizeof(boot));
    boot.magic = SYLE_BOOT_MAGIC; boot.version = SYLE_BOOT_VERSION;
    boot.size = sizeof(boot); boot.boot_services_exited = 1;
    boot.memory_map = (uintptr_t)map; boot.memory_map_bytes = sizeof(map);
    boot.map_buffer = (struct syle_range){(uintptr_t)map, sizeof(map)};
    boot.descriptor_size = sizeof(map[0]); boot.descriptor_version = 1;
}
static void setup_model(void) {
    boot_header();
    map[0] = (struct descriptor){7, 0, LOW, 0, 64, 8};
    map[1] = (struct descriptor){7, 0, CROSS, 0, 64, 8};
    map[2] = (struct descriptor){7, 0, HIGH1, 0, 32, 8};
    map[3] = (struct descriptor){7, 0, HIGH32, 0, 32, 8};
    map[4] = (struct descriptor){7, 0, 0x800000, 0, 4, 8};
    map[5] = (struct descriptor){7, 0, 0xa00000, 0, 4, 8};
    map[6] = (struct descriptor){7, 0, 0xc00000, 0, 4, 8};
    map[7] = (struct descriptor){7, 0, 0xe00000, 0, 4, (UINT64_C(1) << 63) | 8};
    map[8] = (struct descriptor){7, 0, 0xf00000, 0, 4, 0x1008};
    map[9] = (struct descriptor){7, 0, 0x1100000, 0, 4, 0x2008};
    map[10] = (struct descriptor){7, 0, 0x1200000, 0, 4, 0x20008};
    map[11] = (struct descriptor){7, 0, 0x1300000, 0, 4, 1};
    map[12] = (struct descriptor){4, 0, 0x1400000, 0, 4, 8};
    map[13] = (struct descriptor){7, 0, 0x80000, 0, 4, 8};
    map[14] = (struct descriptor){0, 0, 0x1500000, 0, 4, 8};
    boot.image = (struct syle_range){LOW + 3 * PAGE + 7, PAGE};
    boot.kernel_image = (struct syle_range){LOW + 10 * PAGE, 2 * PAGE};
    boot.user_image_buffer = (struct syle_range){LOW + 20 * PAGE + 1, 2 * PAGE};
    boot.user_image = (struct syle_range){LOW + 21 * PAGE + 1, 31};
    boot.framebuffer.base = LOW + 30 * PAGE - 1; boot.framebuffer.size = 2;
    boot.acpi_rsdp = 0x803fff; boot.inherited_rsp = 0xa00011;
    boot.inherited_cr3 = 0xc00456;
    add_pages(LOW, 64); add_pages(CROSS, 64);
    add_pages(HIGH1, 32); add_pages(HIGH32, 32);
    /* Explicit expected reservation pages, independent of reserve() rounding. */
    const unsigned reserved[] = {3, 4, 10, 11, 20, 21, 22, 29, 30};
    for (unsigned i = 0; i < sizeof(reserved) / sizeof(*reserved); ++i)
        model[reserved[i]].eligible = 0;
    assert(used == MODEL_PAGES);
    /* Descriptor order is not part of the firmware contract. */
    for (unsigned i = 14; i; --i) {
        unsigned j = next_random() % (i + 1);
        struct descriptor swap = map[i]; map[i] = map[j]; map[j] = swap;
    }
    assert(pmm_init(&boot));
    assert(pmm_address_limit() == PMM_DIRECT_MAP_LIMIT);
}
static int index_of(uint64_t address) {
    for (unsigned i = 0; i < used; ++i)
        if (model[i].address == address) return (int)i;
    return -1;
}
static void check_stats(void) {
    uint64_t eligible = 0, allocated = 0;
    for (unsigned i = 0; i < used; ++i) {
        eligible += model[i].eligible;
        allocated += model[i].allocated;
        assert(!model[i].allocated || model[i].eligible);
    }
    struct pmm_stats actual = pmm_get_stats();
    assert(actual.usable_pages == eligible);
    assert(actual.allocated_pages == allocated);
    assert(actual.free_pages == eligible - allocated);
}
static uint64_t allocate(unsigned kind, uint64_t bound) {
    uint64_t min = kind == 2 ? bound : 0;
    uint64_t max = kind == 1 ? bound : PMM_DIRECT_MAP_LIMIT;
    int possible = 0;
    for (unsigned i = 0; i < used; ++i)
        if (model[i].eligible && !model[i].allocated && model[i].address >= min &&
            model[i].address < max && max - model[i].address >= PAGE) possible = 1;
    ++calls;
    uint64_t address = kind == 1 ? pmm_alloc_page_below(bound) :
                       kind == 2 ? pmm_alloc_page_above(bound) : pmm_alloc_page();
    if (!address) assert(!possible); /* Metadata cannot fill in this small model. */
    else {
        int i = index_of(address);
        assert(possible && i >= 0 && model[i].eligible && !model[i].allocated);
        assert(address >= min && address < max && max - address >= PAGE);
        model[i].allocated = 1;
    }
    check_stats(); return address;
}
static void release(uint64_t address) {
    int i = index_of(address);
    int expected = i >= 0 && model[i].eligible && model[i].allocated;
    ++calls; assert(pmm_free_page(address) == expected);
    if (expected) model[i].allocated = 0;
    check_stats();
}
static uint64_t random_bound(void) {
    uint32_t choice = next_random();
    if (choice % 11 == 0) return UINT64_MAX;
    if (choice % 11 == 1) return 0;
    if (choice % 11 == 2) return PMM_DIRECT_MAP_LIMIT;
    uint64_t address = model[next_random() % used].address;
    switch (choice % 5) {
    case 0: return address - 1;
    case 1: return address;
    case 2: return address + 1;
    case 3: return address + PAGE - 1;
    default: return address + PAGE;
    }
}
static void run_model(uint32_t seed) {
    random_state = seed; setup_model(); check_stats();
    for (unsigned step = 0; step < 25000; ++step) {
        switch (next_random() % 8) {
        case 0: (void)allocate(0, 0); break;
        case 1: (void)allocate(1, random_bound()); break;
        case 2: (void)allocate(2, random_bound()); break;
        case 3: release(model[next_random() % used].address); break;
        case 4: release(model[next_random() % used].address + 1); break;
        case 5: {
            const uint64_t invalid[] = {0, UINT64_MAX, PMM_DIRECT_MAP_LIMIT,
                0x500000, 0x800000, 0xa00000, 0xc00000, 0xe00000, 0xf00000,
                0x1100000, 0x1200000, 0x1300000, 0x1400000, 0x80000, 0x1500000};
            release(invalid[next_random() % (sizeof(invalid) / sizeof(*invalid))]);
            break;
        }
        case 6: {
            uint64_t address = allocate(0, 0);
            if (address) { release(address); release(address); }
            break;
        }
        case 7:
            ++calls; assert(!pmm_init(next_random() & 1 ? &boot : NULL));
            check_stats(); break;
        }
    }
    /* Exhaustion and final cleanup prove all live ownership, not just counts. */
    while (allocate(0, 0)) {}
    for (unsigned i = 0; i < used; ++i)
        if (model[i].allocated) release(model[i].address);
    assert(pmm_get_stats().free_pages == 183 && !pmm_get_stats().allocated_pages);
    printf("PASS PMM model seed=%u steps=25000 API_calls=%u eligible_pages=183\n", seed, calls);
}
static void run_split_capacity(void) {
    boot_header(); boot.memory_map_bytes = sizeof(map[0]);
    const uint64_t base = UINT64_C(0x100000000);
    const unsigned prefix = 2 * (PMM_MAX_RANGES - 1);
    map[0] = (struct descriptor){7, 0, base, 0, prefix + 3, 8};
    assert(pmm_init(&boot));
    for (unsigned i = 0; i < prefix; ++i)
        assert(pmm_alloc_page() == base + i * PAGE);
    for (unsigned i = 0; i < PMM_MAX_RANGES - 1; ++i)
        assert(pmm_free_page(base + 2 * i * PAGE));
    /* 4095 isolated free pages + a 3-page tail consume all metadata. */
    uint64_t middle = base + (prefix + 1) * PAGE;
    struct pmm_stats before = pmm_get_stats();
    assert(!pmm_alloc_page_above(middle)); /* Would need a 4097th free extent. */
    struct pmm_stats after = pmm_get_stats();
    assert(before.usable_pages == after.usable_pages &&
           before.free_pages == after.free_pages &&
           before.allocated_pages == after.allocated_pages);
    assert(!pmm_free_page(middle)); /* Failed allocation did not transfer it. */
    assert(pmm_alloc_page() == base); /* Remove one single-page free extent. */
    assert(pmm_alloc_page_above(middle) == middle); /* Split now fits. */
    assert(pmm_free_page(middle)); /* Coalesce both tail neighbors. */
    assert(pmm_free_page(base));
    for (unsigned i = 1; i < prefix; i += 2)
        assert(pmm_free_page(base + i * PAGE));
    assert(pmm_get_stats().free_pages == prefix + 3 &&
           !pmm_get_stats().allocated_pages);
    /* Check every recovered page once after the capacity failure. */
    for (unsigned i = 0; i < prefix + 3; ++i)
        assert(pmm_alloc_page() == base + i * PAGE);
    assert(!pmm_alloc_page());
    puts("PASS PMM allocation-split capacity: unchanged ownership/stats, retry, coalescing and complete reallocation");
}
int main(int argc, char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1], "split-capacity")) run_split_capacity();
    else {
        char *end; unsigned long seed = strtoul(argv[1], &end, 0);
        assert(*argv[1] && !*end && seed && seed <= UINT32_MAX);
        run_model((uint32_t)seed);
    }
}
