#include "../../include/syle/boot_validate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct syle_boot_info good(void) {
    struct syle_boot_info b={0};
    b.magic=SYLE_BOOT_MAGIC; b.version=SYLE_BOOT_VERSION; b.size=sizeof(b);
    b.boot_services_exited=1; b.descriptor_size=48; b.descriptor_version=1;
    b.memory_map=0x200000; b.memory_map_bytes=96;
    b.map_buffer=(struct syle_range){0x200000,4096};
    b.image=(struct syle_range){0x300000,65536};
    b.kernel_image=(struct syle_range){0x4000000,4096}; b.kernel_entry=0x4000000;
    b.inherited_stack_and_tables=1;
    return b;
}

/* Integer metadata only. These helpers never dereference range addresses. */
static void range_boundaries(void) {
    const struct { struct syle_range outer, inner; int expected; } cases[]={
        {{100,20},{100,20},1}, /* Exact match. */
        {{100,20},{100,1},1}, {{100,20},{119,1},1},
        {{100,20},{120,0},1}, /* Empty range at the exclusive endpoint. */
        {{100,20},{99,1},0},  /* Starts before outer. */
        {{100,20},{100,21},0}, {{100,20},{120,1},0},
        {{100,20},{121,0},0}, {{100,0},{100,0},1},
        {{100,0},{100,1},0}, {{0,0},{0,0},1},
        {{UINT64_MAX-10,10},{UINT64_MAX,0},1},
        {{UINT64_MAX-10,10},{UINT64_MAX,1},0}, /* Inner sum overflows. */
        {{UINT64_MAX,1},{UINT64_MAX,0},0},   /* Outer sum overflows. */
        {{0,UINT64_MAX},{0,UINT64_MAX},1},
    };
    for (unsigned i=0; i<sizeof cases/sizeof cases[0]; ++i)
        assert(syle_boot_range_contains(cases[i].outer,cases[i].inner)==cases[i].expected);
    assert(syle_boot_range_valid((struct syle_range){UINT64_MAX,0}));
    assert(!syle_boot_range_valid((struct syle_range){UINT64_MAX,1}));
}
static void accepted_boundaries(void) {
    /* Descriptor minimum and larger, 8-byte-aligned versions of the V1 stride. */
    const unsigned strides[]={40,48,56,64};
    for (unsigned i=0; i<sizeof strides/sizeof strides[0]; ++i) {
        struct syle_boot_info b=good();
        b.descriptor_size=strides[i]; b.memory_map_bytes=2*strides[i];
        b.map_buffer.size=b.memory_map_bytes;
        assert(syle_boot_info_valid(&b));
        b.map_buffer.size--; assert(!syle_boot_info_valid(&b));
    }
    struct syle_boot_info b=good();
    b.user_image_buffer=(struct syle_range){0x500000,4096};
    b.user_image=b.user_image_buffer; assert(syle_boot_info_valid(&b));
    b.user_image=(struct syle_range){0x500fff,1}; assert(syle_boot_info_valid(&b));
    b.user_image.base++; assert(!syle_boot_info_valid(&b));
    b.user_image=(struct syle_range){0x4fffff,1}; assert(!syle_boot_info_valid(&b));
    b=good(); b.ap_trampoline=(struct syle_range){0x8000,4096};
    assert(syle_boot_info_valid(&b));
    b.boot_services_exited=2; assert(!syle_boot_info_valid(&b));
    b=good(); b.inherited_stack_and_tables=2; assert(!syle_boot_info_valid(&b));
    b=good(); b.size++; assert(!syle_boot_info_valid(&b));
    /* Unknown future record version is not accepted as the current contract. */
    b=good(); b.version=SYLE_BOOT_VERSION+1; assert(!syle_boot_info_valid(&b));
}
int main(void) {
    range_boundaries(); accepted_boundaries();
    struct syle_boot_info b=good(); unsigned rejected=0;
    assert(syle_boot_info_valid(&b)); assert(!syle_boot_info_valid(0));
#define BAD(...) do { b=good(); __VA_ARGS__; assert(!syle_boot_info_valid(&b)); ++rejected; } while(0)
    BAD(b.magic=0); BAD(b.version=3); BAD(b.size--);
    BAD(b.boot_services_exited=0); BAD(b.mode=2); BAD(b.uart_present=2);
    BAD(b.memory_map=0); BAD(b.memory_map_bytes=0);
    BAD(b.descriptor_size=39); BAD(b.descriptor_size=41);
    BAD(b.descriptor_version=2); BAD(b.memory_map_bytes++);
    BAD(b.map_buffer.base++); BAD(b.map_buffer.size=95);
    BAD(b.memory_map=UINT64_MAX-15; b.map_buffer.base=b.memory_map);
    BAD(b.map_buffer.size=UINT64_MAX);
    BAD(b.image.base=0); BAD(b.image.size=0); BAD(b.image.size=UINT64_MAX);
    BAD(b.kernel_image.base++); BAD(b.kernel_image.size=0);
    BAD(b.kernel_image.size=8*1024*1024+4096); BAD(b.kernel_image.size++);
    BAD(b.kernel_entry--); BAD(b.kernel_entry+=4096);
    BAD(b.user_image=(struct syle_range){UINT64_MAX,2});
    BAD(b.user_image_buffer=(struct syle_range){UINT64_MAX,2});
    BAD(b.user_image=(struct syle_range){0,1});
    BAD(b.user_image=(struct syle_range){0x500000,1});
    BAD(b.ap_trampoline=(struct syle_range){UINT64_MAX,2});
    BAD(b.inherited_stack_and_tables=0); BAD(b.reclaim_allowed=1);
    b=good(); b.user_image=(struct syle_range){0x500010,12};
    b.user_image_buffer=(struct syle_range){0x500000,4096};
    assert(syle_boot_info_valid(&b));
    b.user_image.size=4096; assert(!syle_boot_info_valid(&b));
    b=good(); b.mode=1; b.uart_present=1;
    b.kernel_image.size=8*1024*1024; b.kernel_entry+=8*1024*1024-1;
    assert(syle_boot_info_valid(&b));
    /* Optional GOP fields do not make the entire handoff mandatory. Their
     * bounds are checked by console_bind, never dereferenced by this helper. */
    b.framebuffer=(struct syle_framebuffer){.base=UINT64_MAX,.size=2,.valid=1};
    assert(syle_boot_info_valid(&b));
    printf("BOOT_INFO.V4_PASS legacy_rejected=%u optional_gop_degrades=1 range_cases=15 descriptor_boundaries=4\n",rejected+2);
}
