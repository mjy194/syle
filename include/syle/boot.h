#ifndef SYLE_BOOT_H
#define SYLE_BOOT_H
#include <stdint.h>
#define SYLE_BOOT_MAGIC UINT64_C(0x53594c45424f4f54)
#define SYLE_BOOT_VERSION 4u
/* All addresses are physical/identity-mapped in this x86-64 prototype.
 * No memory reclamation is permitted: inherited firmware allocations stay reserved.
 * Kernel entry replaces its stack; early kernel initialization replaces CR3.
 * No EFI service pointers cross this boundary. */
struct syle_range { uint64_t base, size; };
struct syle_framebuffer {
    uint64_t base, size;
    uint32_t width, height, stride, format; /* 0 RGBX, 1 BGRX */
    uint32_t valid, reserved;
};
struct syle_boot_info {
    uint64_t magic;
    uint32_t version, size;
    uint64_t memory_map, memory_map_bytes, descriptor_size;
    uint32_t descriptor_version, boot_services_exited;
    struct syle_range image; /* EFI loader, boot info and live inherited state */
    struct syle_range kernel_image; /* independent loaded kernel allocation */
    uint64_t kernel_entry;
    struct syle_range user_image; /* ELF bytes loaded before EBS */
    struct syle_range user_image_buffer; /* full reserved allocation */
    uint32_t mode, reserved; /* 0 management, 1 diagnostic */
    struct syle_range map_buffer; /* allocation capacity, not just used bytes */
    struct syle_framebuffer framebuffer;
    uint64_t acpi_rsdp; /* firmware configuration-table pointer; not validated */
    uint32_t acpi_revision, uart_present; /* 0 absent, 1 ACPI1, 2 ACPI2 GUID */
    uint64_t inherited_rsp, inherited_cr3;
    struct syle_range ap_trampoline; /* optional EFI-owned page below 1MiB; never reclaimed */
    uint32_t inherited_stack_and_tables, reclaim_allowed; /* 1, 0 */
};
__attribute__((noreturn)) void syle_main(const struct syle_boot_info *boot);
#endif
