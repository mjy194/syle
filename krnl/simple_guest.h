#ifndef SYLE_SIMPLE_GUEST_H
#define SYLE_SIMPLE_GUEST_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vmx_guest.h"

/* Opt-in technical prototype only. Not a selected OS or stable guest ABI. */
#define SG_PAGE_SIZE 4096u
#define SG_PAGE_COUNT 64u
#define SG_RAM_SIZE (SG_PAGE_COUNT * SG_PAGE_SIZE)
#define SG_IMAGE_MIN UINT64_C(0x10000)
#define SG_MAX_IMAGE (256u * 1024u)
#define SG_MAX_SEGMENTS 8u
#define SG_CR3 UINT64_C(0x1000)
#define SG_GDT UINT64_C(0x5000)
#define SG_TSS UINT64_C(0x5100)
#define SG_IDT UINT64_C(0x5800)
#define SG_BOOT_INFO UINT64_C(0x6000)
#define SG_STACK_TOP UINT64_C(0x8000)
#define SG_BOOT_MAGIC UINT64_C(0x53474c4f41443031)
#define SG_BOOT_VERSION 1u
#define SG_EXIT_LIMIT 256u
/* Per-entry host-TSC tick budget. Runtime converts by VMX_MISC rate, rejects
 * rates above20, and never treats this as a wall-clock duration guarantee. */
#define SG_ENTRY_TICKS UINT32_C(0x10000000)

struct sg_segment {
    uint64_t offset, address, filesz, memsz;
    uint32_t flags;
};
struct sg_image {
    uint64_t entry;
    unsigned count;
    struct sg_segment segments[SG_MAX_SEGMENTS];
};
struct sg_boot_record {
    uint64_t magic;
    uint32_t version, size;
    uint64_t ram_bytes, stack_top, image_entry;
};
_Static_assert(sizeof(struct sg_boot_record) == 40, "prototype boot record ABI");
/* Callback allocator is trusted, returns distinct owned physical pages. */
struct sg_memory_ops {
    void *context;
    uint64_t (*allocate)(void *);
    bool (*release)(void *, uint64_t);
    void *(*map)(void *, uint64_t);
    bool (*writeback)(void *, uint64_t);
};
struct sg_memory {
    uint64_t pages[SG_PAGE_COUNT], tables[4], eptp;
    uint8_t permissions[SG_PAGE_COUNT]; /* EPT R/W/X bits, 0 means absent. */
    struct sg_image image;
    bool prepared;
};
/* Validation is atomic; invalid input leaves *out unchanged. */
bool sg_image_inspect(const void *, size_t, struct sg_image *out);
/* Fresh zeroed object only. Failure unwinds all allocations; no VMX performed.
 * On callback release failure returns false with remaining ownership retained. */
bool sg_memory_prepare(struct sg_memory *, const struct sg_memory_ops *, const void *, size_t);
/* Caller must prove VMCS clear, INVEPT complete and VMXOFF before release if used.
 * Never call on any active context. Remaining failed frees remain recorded. */
bool sg_memory_release(struct sg_memory *, const struct sg_memory_ops *);
bool sg_image_executable(const struct sg_image *, uint64_t rip, uint64_t length);

enum sg_action { SG_REJECT, SG_RESUME, SG_PUTCHAR, SG_FINISH };
struct sg_run_state { unsigned exits, characters; bool finished, failed; uint64_t exit_code; };
/* Exit policy is testable without VMX. Unsupported exits fail closed.
 * VMCALL RAX=1 prints low-byte RBX, RAX=2 finishes with status in RBX.
 * CPUID follows the existing deliberately restricted synthetic profile. */
enum sg_action sg_dispatch(struct sg_run_state *, const struct sg_image *,
    uint64_t reason, uint64_t rip, uint64_t length,
    struct vmx_guest_registers *, uint64_t *next_rip, uint8_t *character);
/* Strict byte IN/OUT DX route for the emulated COM1 only. Qualification is
 * hardware-provided. Rejection is sticky but does not modify UART/GPR/output. */
struct guest_uart;
enum sg_action sg_dispatch_io(struct sg_run_state *, const struct sg_image *,
    struct guest_uart *, uint64_t qualification, uint64_t rip, uint64_t length,
    struct vmx_guest_registers *, uint64_t *next_rip, uint8_t *character);
/* Runtime image binding, before vmx_probe; ELF bytes remain loader-owned. */
bool simple_guest_bind(const void *, size_t);
bool simple_guest_succeeded(void);
#endif
