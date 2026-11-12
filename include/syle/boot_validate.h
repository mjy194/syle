#ifndef SYLE_BOOT_VALIDATE_H
#define SYLE_BOOT_VALIDATE_H
#include "boot.h"

/* Validate the contents of a loader-owned, readable V4 record before following
 * any of its pointers. This cannot prove an arbitrary boot pointer is mapped;
 * that remains the firmware-to-kernel handoff precondition. Optional display
 * metadata is independently validated by console_bind and may be disabled. */
static inline int syle_boot_range_valid(struct syle_range r) {
    return r.size <= UINT64_MAX - r.base;
}
static inline int syle_boot_range_contains(struct syle_range outer,
                                          struct syle_range inner) {
    return syle_boot_range_valid(outer) && syle_boot_range_valid(inner) &&
           inner.base >= outer.base && inner.size <= outer.size &&
           inner.base - outer.base <= outer.size - inner.size;
}
static inline int syle_boot_info_valid(const struct syle_boot_info *b) {
    if (!b || b->magic != SYLE_BOOT_MAGIC || b->version != SYLE_BOOT_VERSION ||
        b->size != sizeof(*b) || b->boot_services_exited != 1 || b->mode > 1 ||
        b->uart_present > 1 || !b->memory_map || !b->memory_map_bytes ||
        b->descriptor_size < 40 || b->descriptor_size % 8 ||
        b->descriptor_version != 1 ||
        b->memory_map_bytes % b->descriptor_size ||
        b->map_buffer.base != b->memory_map ||
        !syle_boot_range_contains(b->map_buffer,
            (struct syle_range){b->memory_map, b->memory_map_bytes}) ||
        !b->image.base || !b->image.size || !syle_boot_range_valid(b->image) ||
        b->kernel_image.base != UINT64_C(0x04000000) ||
        !b->kernel_image.size || b->kernel_image.size > 8*1024*1024 ||
        b->kernel_image.size % 4096 ||
        b->kernel_entry < b->kernel_image.base ||
        b->kernel_entry - b->kernel_image.base >= b->kernel_image.size ||
        !syle_boot_range_valid(b->user_image) ||
        !syle_boot_range_valid(b->user_image_buffer) ||
        (b->user_image.size && (!b->user_image.base ||
            !syle_boot_range_contains(b->user_image_buffer,b->user_image))) ||
        !syle_boot_range_valid(b->ap_trampoline) ||
        b->inherited_stack_and_tables != 1 || b->reclaim_allowed != 0)
        return 0;
    return 1;
}
#endif
