/* Host-only: include the actual EFI adapter and its diagnostic call sites.
 * No efi_main/platform_launch execution, UART, CR registers or kernel entry. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <efi.h>
#include <efilib.h>
#undef uefi_call_wrapper
#define uefi_call_wrapper(function, count, ...) function(__VA_ARGS__)
#include "../src/main.c"

static char output[8192], events[128];
static size_t output_used, event_count;
static unsigned map_calls, exit_calls, allocation_calls, releases, page_releases;
static EFI_STATUS map_status[8], exit_status[4], allocate_status, reserve_status;
static size_t map_stride[8], map_bytes[8];
static uint32_t map_version[8];
static unsigned map_count, exit_count, allocation_failure_call;
static int null_allocation, mutate_address;
static uint64_t requested_base, released_base;
static size_t requested_pages, released_pages;
static unsigned char pool[8192];
static EFI_BOOT_SERVICES services;
static EFI_SYSTEM_TABLE system_table;
static struct efi_context context;
static void event(char c) {
    assert(event_count + 1 < sizeof(events));
    /* Logs only after the transaction: no output in the map-to-EBS window. */
    assert(output_used == 0);
    if (exit_calls) assert(c == 'M' || c == 'E');
    events[event_count++] = c;
    events[event_count] = 0;
}
void console_putc(char c) {
    assert(output_used + 1 < sizeof(output));
    output[output_used++] = c;
    output[output_used] = 0;
}
static EFI_STATUS EFIAPI mock_map(UINTN *bytes, EFI_MEMORY_DESCRIPTOR *map,
                                  UINTN *key, UINTN *stride, UINT32 *version) {
    event('M');
    unsigned i = map_calls++;
    assert(i < map_count);
    assert(map == (i ? (EFI_MEMORY_DESCRIPTOR *)pool : NULL));
    *bytes = map_bytes[i]; *stride = map_stride[i]; *version = map_version[i];
    *key = i + 123;
    return map_status[i];
}
static EFI_STATUS EFIAPI mock_allocate(EFI_MEMORY_TYPE type, UINTN bytes, void **map) {
    event('A'); ++allocation_calls;
    assert(type == EfiLoaderData &&
           bytes == map_bytes[map_calls - 1] + 32 * map_stride[map_calls - 1]);
    *map = null_allocation ? NULL : pool;
    return allocation_calls == allocation_failure_call ? allocate_status : EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_free(void *map) {
    event('F'); ++releases; assert(map == pool);
    return EFI_DEVICE_ERROR; /* Cleanup must not overwrite the original cause. */
}
static EFI_STATUS EFIAPI mock_exit(EFI_HANDLE image, UINTN key) {
    event('E'); assert(image == context.image && key == map_calls - 1 + 123);
    assert(exit_calls < exit_count);
    return exit_status[exit_calls++];
}
static EFI_STATUS EFIAPI mock_reserve(EFI_ALLOCATE_TYPE type, EFI_MEMORY_TYPE memory,
                                      UINTN pages, EFI_PHYSICAL_ADDRESS *address) {
    event('P'); assert(type == AllocateAddress && memory == EfiLoaderCode);
    requested_base = *address; requested_pages = pages;
    if (mutate_address) *address = 0xdead000;
    return reserve_status;
}
static EFI_STATUS EFIAPI mock_release(EFI_PHYSICAL_ADDRESS address, UINTN pages) {
    event('R'); ++page_releases; released_base = address; released_pages = pages;
    return EFI_DEVICE_ERROR;
}
static void reset(void) {
    output_used = event_count = 0; output[0] = events[0] = 0;
    map_calls = exit_calls = allocation_calls = releases = page_releases = 0;
    null_allocation = mutate_address = 0;
    memset(map_status, 0, sizeof(map_status)); memset(exit_status, 0, sizeof(exit_status));
    for (unsigned i = 0; i < 8; ++i) {
        map_stride[i] = map_bytes[i] = 40; map_version[i] = 1;
    }
    map_status[0] = EFI_BUFFER_TOO_SMALL;
    map_count = 2; exit_count = 1; allocation_failure_call = 1;
    allocate_status = reserve_status = EFI_SUCCESS;
    services = (EFI_BOOT_SERVICES){0};
    services.GetMemoryMap = mock_map; services.AllocatePool = mock_allocate;
    services.FreePool = mock_free; services.ExitBootServices = mock_exit;
    services.AllocatePages = mock_reserve; services.FreePages = mock_release;
    system_table = (EFI_SYSTEM_TABLE){.BootServices = &services};
    context = (struct efi_context){.image = (void *)0x1234, .system = &system_table};
}
static struct syle_exit_result run(int expected, const char *order) {
    struct syle_exit_ops ops = {&context, final_map, final_allocate, final_release, final_exit};
    struct syle_exit_result result;
    assert(syle_exit_services(&ops, &result) == expected);
    assert(strcmp(events, order) == 0);
    if (expected) final_failure(&context, &result);
    return result;
}
static void contains(const char *s) { assert(strstr(output, s)); }
static void raw(EFI_STATUS status) {
    char expected[64];
    snprintf(expected, sizeof(expected), "FINAL.EFI_STATUS=0x%016lx\n", (unsigned long)status);
    contains(expected);
}
static void test_exit(void) {
    struct syle_exit_result r;
    reset(); r = run(0, "MAME"); assert(r.exited && r.attempted == 1 && !releases);
    reset(); map_status[0] = EFI_DEVICE_ERROR; map_count = 1;
    r = run(-1, "M"); assert(!r.attempted); raw(EFI_DEVICE_ERROR);
    contains("ERROR.FINAL_MEMORY_MAP\nFINAL.STAGE=GetMemoryMap\nFINAL.CAUSE=firmware-error\n");
    reset(); map_status[1] = EFI_ACCESS_DENIED;
    r = run(-1, "MAMF"); assert(!r.map && !r.capacity && releases == 1);
    raw(EFI_ACCESS_DENIED); assert(context.final_stage == FINAL_MAP);
    reset(); allocate_status = EFI_OUT_OF_RESOURCES;
    r = run(-1, "MA"); assert(!r.attempted && !releases); raw(EFI_OUT_OF_RESOURCES);
    contains("FINAL.STAGE=AllocatePool\nFINAL.CAUSE=firmware-error\n");
    reset(); allocate_status = EFI_BUFFER_TOO_SMALL;
    r = run(-1, "MA"); assert(!r.attempted && !releases); raw(EFI_BUFFER_TOO_SMALL);
    contains("FINAL.STAGE=AllocatePool\nFINAL.CAUSE=firmware-error\n");
    reset(); allocate_status = EFI_OUT_OF_RESOURCES; allocation_failure_call = 2;
    map_status[1] = EFI_BUFFER_TOO_SMALL; map_bytes[1] = 2048;
    r = run(-1, "MAMAF"); assert(!r.attempted && !r.map && releases == 1);
    raw(EFI_OUT_OF_RESOURCES); contains("FINAL.STAGE=AllocatePool\nFINAL.CAUSE=firmware-error\n");
    reset(); null_allocation = 1;
    r = run(-1, "MA"); assert(!r.attempted); raw(EFI_SUCCESS);
    contains("FINAL.STAGE=AllocatePool\nFINAL.CAUSE=transaction-rejected\n");
    reset(); map_stride[1] = 41;
    r = run(-1, "MAMF"); assert(!r.attempted && releases == 1); raw(EFI_SUCCESS);
    contains("FINAL.STAGE=GetMemoryMap\nFINAL.CAUSE=transaction-rejected\n");
    reset(); map_stride[0] = 0;
    r = run(-1, "M"); assert(!r.attempted); raw(EFI_BUFFER_TOO_SMALL);
    contains("FINAL.CAUSE=transaction-rejected\n");
    reset(); exit_status[0] = EFI_DEVICE_ERROR;
    r = run(-1, "MAME"); assert(r.attempted == 1 && r.map == pool && !releases);
    raw(EFI_DEVICE_ERROR); contains("ERROR.EXIT_BOOT_SERVICES\nFINAL.STAGE=ExitBootServices\n");
    reset(); exit_status[0] = EFI_BUFFER_TOO_SMALL;
    r = run(-1, "MAME"); assert(r.attempted == 1 && r.map == pool && !releases);
    raw(EFI_BUFFER_TOO_SMALL); contains("FINAL.STAGE=ExitBootServices\nFINAL.CAUSE=firmware-error\n");
    reset(); exit_status[0] = EFI_INVALID_PARAMETER; map_count = 3; exit_count = 2;
    r = run(0, "MAMEME"); assert(r.exited && r.attempted == 2 && !releases);
    reset(); exit_status[0] = EFI_INVALID_PARAMETER; exit_status[1] = EFI_ACCESS_DENIED;
    map_count = 3; exit_count = 2;
    r = run(-1, "MAMEME"); assert(r.attempted == 2 && !releases);
    raw(EFI_ACCESS_DENIED); contains("FINAL.STAGE=ExitBootServices\n");
    reset(); map_count = 4; exit_count = 3;
    exit_status[0] = exit_status[1] = exit_status[2] = EFI_INVALID_PARAMETER;
    r = run(-1, "MAMEMEME"); assert(r.attempted == 3 && !r.exited && !releases);
    raw(EFI_INVALID_PARAMETER); contains("FINAL.STAGE=ExitBootServices\n");
    reset(); exit_status[0] = EFI_INVALID_PARAMETER; map_count = 3;
    map_status[2] = EFI_ACCESS_DENIED;
    r = run(-1, "MAMEM"); assert(r.attempted == 1 && !releases);
    raw(EFI_ACCESS_DENIED); contains("ERROR.EXIT_BOOT_SERVICES\nFINAL.STAGE=GetMemoryMap\n");
    assert(!strstr(output, "0x8000000000000002")); /* No obsolete stale status. */
    reset(); exit_status[0] = EFI_INVALID_PARAMETER; map_count = 3;
    map_status[2] = EFI_BUFFER_TOO_SMALL; map_bytes[2] = 4096;
    r = run(-1, "MAMEM"); assert(r.attempted == 1 && allocation_calls == 1 && !releases);
    raw(EFI_BUFFER_TOO_SMALL); contains("FINAL.CAUSE=transaction-rejected\n");
    reset(); exit_status[0] = EFI_INVALID_PARAMETER; map_count = 3; map_version[2] = 2;
    r = run(-1, "MAMEM"); assert(r.attempted == 1 && !releases); raw(EFI_SUCCESS);
    contains("FINAL.CAUSE=transaction-rejected\n");
}
static void test_kernel(const void *file, size_t bytes) {
    struct syle_kernel_plan plan, inspected;
    assert(syle_kernel_elf_inspect(file, bytes, &inspected) == 0);
    const EFI_STATUS failures[] = {EFI_OUT_OF_RESOURCES, EFI_ACCESS_DENIED};
    for (unsigned i = 0; i < sizeof(failures) / sizeof(*failures); ++i) {
        reset(); mutate_address = 1; reserve_status = failures[i];
        const struct syle_kernel_allocator ops = {&context, kernel_reserve, kernel_release};
        int result = syle_kernel_prepare(file, bytes, &ops, &plan);
        assert(result == -2 && strcmp(events, "P") == 0 && !page_releases);
        assert(context.kernel_status == failures[i]);
        assert(context.kernel_requested_base == inspected.base && requested_base == inspected.base);
        assert(context.kernel_requested_size == inspected.size && requested_pages == inspected.size / 4096);
        kernel_failure(&context, result);
        contains("ERROR.KERNEL_ADDRESS_UNAVAILABLE\n");
        char expected[96];
        snprintf(expected, sizeof(expected), "KERNEL.ALLOCATE_PAGES status=0x%016lx\n", (unsigned long)failures[i]);
        contains(expected);
        snprintf(expected, sizeof(expected), "KERNEL.REQUESTED_BASE=0x%016lx\n", (unsigned long)inspected.base);
        contains(expected);
        snprintf(expected, sizeof(expected), "KERNEL.REQUESTED_SIZE=0x%016lx\n", (unsigned long)inspected.size);
        contains(expected); assert(!strstr(output, "0x000000000dead000"));
    }
    reset(); mutate_address = 1;
    const struct syle_kernel_allocator ops = {&context, kernel_reserve, kernel_release};
    int result = syle_kernel_prepare(file, bytes, &ops, &plan);
    assert(result == -3 && strcmp(events, "PR") == 0);
    assert(page_releases == 1 && released_base == 0xdead000 && released_pages == inspected.size / 4096);
    kernel_failure(&context, result); contains("ERROR.KERNEL_LOAD\n");
    assert(!strstr(output, "KERNEL.ALLOCATE_PAGES"));
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long length = ftell(f); assert(length > 0);
    rewind(f); void *file = malloc((size_t)length); assert(file);
    assert(fread(file, 1, (size_t)length, f) == (size_t)length); fclose(f);
    test_exit(); test_kernel(file, (size_t)length); free(file);
    puts("firmware diagnostics: 17 final-map/EBS scenarios and 3 kernel adapter scenarios passed");
    return 0;
}
