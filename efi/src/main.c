#include <efi.h>
#include <efilib.h>
#include "console.h"
#include "exceptions.h"
#include "../../include/syle/boot.h"
#include "../../include/syle/boot_platform.h"
#include "../../boot/kernel_elf.h"
#include "../../boot/exit_services.h"
#include "../../boot/kernel_prepare.h"
#include "../../boot/read_bounded.h"
#ifdef SIMPLE_GUEST_PROTOTYPE
#include "../../krnl/simple_guest.h"
#endif

/* Stage 0 is a loader/debug harness, not yet a VMX hypervisor. */
static inline void out8(UINT16 port, UINT8 value) {
    __asm__ volatile ("outb %0,%1" : : "a"(value), "Nd"(port));
}
static inline UINT8 in8(UINT16 port) {
    UINT8 value;
    __asm__ volatile ("inb %1,%0" : "=a"(value) : "Nd"(port));
    return value;
}
static int uart_present;
static void serial_init(void) {
    /* QEMU COM1 prototype: absent legacy UART is allowed. No physical probing claim. */
    if (in8(0x3fd) == 0xff) return;
    out8(0x3ff, 0x5a);
    if (in8(0x3ff) != 0x5a) return;
    uart_present = 1;
    out8(0x3f9, 0); out8(0x3fb, 0x80);
    out8(0x3f8, 1); out8(0x3f9, 0);
    out8(0x3fb, 3); out8(0x3fa, 0xc7); out8(0x3fc, 0x0b);
}
static void serial_putc(char c) {
    if (!uart_present) return;
    /* Bound polling so an absent UART cannot hang the loader. */
    for (UINTN i = 0; i < 10000; ++i) {
        if (in8(0x3fd) & 0x20) { out8(0x3f8, (UINT8)c); return; }
    }
    /* Stop retrying a wedged transmitter on every subsequent character. */
    uart_present = 0;
}
static void log_char(char c) {
    /* Commit serial output before touching a potentially broken framebuffer. */
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
    console_putc(c);
}
void log_text(const char *s) { while (*s) log_char(*s++); }
void log_hex(UINT64 value) {
    const char *digits = "0123456789abcdef";
    log_text("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        log_char(digits[(value >> shift) & 15]);
    log_text("\n");
}
static void log_cpu(void) {
    UINT32 a = 1, b, c, d;
    __asm__ volatile ("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
    log_text(c & (1u << 5) ? "CPU.VMX=1 (capability only)\n" : "CPU.VMX=0\n");
    log_text(c & (1u << 31) ? "CPU.HYPERVISOR=1\n" : "CPU.HYPERVISOR=0\n");
    log_text("VMX.NOT_ATTEMPTED\n");
}

static struct syle_boot_info boot;
/* Keep the existing GDB breakpoint symbol; immediately enter the kernel. */
__attribute__((noinline, noreturn)) void stage0_post_ebs(void) {
    void (*entry)(const struct syle_boot_info *) = (void *)(UINTN)boot.kernel_entry;
    entry(&boot);
    for (;;) __asm__ volatile("cli; hlt");
}

enum final_stage { FINAL_NONE, FINAL_MAP, FINAL_ALLOCATE, FINAL_EXIT };
struct efi_context {
    EFI_HANDLE image;
    EFI_SYSTEM_TABLE *system;
    /* Only transaction adapters update these; cleanup must not erase cause. */
    enum final_stage final_stage;
    EFI_STATUS final_status;
    EFI_STATUS kernel_status;
    uint64_t kernel_requested_base;
    size_t kernel_requested_size;
};
static struct efi_context platform_context;

/* Files are read only while Boot Services are live, from this image's device.
 * Never retain protocol pointers in boot_info. All sizes are bounded. */
static int read_stream(void *context, void *buffer, size_t *bytes) {
    EFI_FILE_HANDLE file = context;
    return EFI_ERROR(uefi_call_wrapper(file->Read, 3, file, bytes, buffer));
}
static EFI_STATUS read_file(struct efi_context *ctx, CHAR16 *path,
                            void *buffer, UINTN capacity, UINTN *used) {
    EFI_LOADED_IMAGE *loaded = NULL;
    EFI_FILE_IO_INTERFACE *fs = NULL;
    EFI_FILE_HANDLE root = NULL, file = NULL;
    EFI_GUID image_guid = LOADED_IMAGE_PROTOCOL;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_BOOT_SERVICES *bs = ctx->system->BootServices;
    EFI_STATUS status = uefi_call_wrapper(bs->HandleProtocol, 3, ctx->image,
                                         &image_guid, (void **)&loaded);
    if (EFI_ERROR(status)) return status;
    status = uefi_call_wrapper(bs->HandleProtocol, 3, loaded->DeviceHandle,
                              &fs_guid, (void **)&fs);
    if (EFI_ERROR(status)) return status;
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) return status;
    status = uefi_call_wrapper(root->Open, 5, root, &file, path, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(status)) {
        status = syle_read_bounded(file, read_stream, buffer, capacity, used)
                 ? EFI_BAD_BUFFER_SIZE : EFI_SUCCESS;
        uefi_call_wrapper(file->Close, 1, file);
    }
    uefi_call_wrapper(root->Close, 1, root);
    return status;
}
static void platform_log(void *context, const char *text) {
    (void)context;
    log_text(text);
}
static int platform_config(void *context, char *buffer, size_t capacity, size_t *used) {
    UINTN bytes = 0;
    EFI_STATUS status = read_file(context, L"\\syle\\boot.cfg", buffer, capacity, &bytes);
    *used = bytes;
    return status == EFI_NOT_FOUND ? 1 : EFI_ERROR(status) ? -1 : 0;
}
static int platform_key(void *context) {
    struct efi_context *ctx = context;
    EFI_INPUT_KEY key;
    if (!ctx->system->ConIn) return -1;
    EFI_STATUS status = uefi_call_wrapper(ctx->system->ConIn->ReadKeyStroke, 2,
                                         ctx->system->ConIn, &key);
    if (status == EFI_NOT_READY) return 0;
    if (EFI_ERROR(status)) return -1;
    return key.UnicodeChar <= 127 ? key.UnicodeChar : 0;
}
static void platform_stall(void *context, unsigned milliseconds) {
    struct efi_context *ctx = context;
    uefi_call_wrapper(ctx->system->BootServices->Stall, 1, (UINTN)milliseconds * 1000);
}
static int final_map(void *context, void *map, size_t *bytes, size_t *key,
                     size_t *stride, uint32_t *version) {
    struct efi_context *ctx = context;
    EFI_STATUS status = uefi_call_wrapper(ctx->system->BootServices->GetMemoryMap,
                                         5, bytes, map, key, stride, version);
    ctx->final_stage = FINAL_MAP;
    ctx->final_status = status;
    return status == EFI_BUFFER_TOO_SMALL ? SYLE_FW_SMALL :
           EFI_ERROR(status) ? SYLE_FW_ERROR : SYLE_FW_OK;
}
static int final_allocate(void *context, size_t bytes, void **map) {
    struct efi_context *ctx = context;
    EFI_STATUS status = uefi_call_wrapper(ctx->system->BootServices->AllocatePool,
                                          3, EfiLoaderData, bytes, map);
    ctx->final_stage = FINAL_ALLOCATE;
    ctx->final_status = status;
    return EFI_ERROR(status);
}
static void final_release(void *context, void *map) {
    struct efi_context *ctx = context;
    uefi_call_wrapper(ctx->system->BootServices->FreePool, 1, map);
}
static int final_exit(void *context, size_t key) {
    struct efi_context *ctx = context;
    EFI_STATUS status = uefi_call_wrapper(ctx->system->BootServices->ExitBootServices,
                                         2, ctx->image, key);
    ctx->final_stage = FINAL_EXIT;
    ctx->final_status = status;
    return status == EFI_INVALID_PARAMETER ? SYLE_FW_STALE :
           EFI_ERROR(status) ? SYLE_FW_ERROR : SYLE_FW_OK;
}
static int kernel_reserve(void *context, uint64_t *address, size_t size, void **destination) {
    struct efi_context *ctx = context;
    ctx->kernel_requested_base = *address;
    ctx->kernel_requested_size = size;
    EFI_STATUS status = uefi_call_wrapper(ctx->system->BootServices->AllocatePages,
                         4, AllocateAddress, EfiLoaderCode, size / 4096, address);
    ctx->kernel_status = status;
    *destination = EFI_ERROR(status) ? NULL : (void *)(UINTN)*address;
    return EFI_ERROR(status);
}
static void kernel_release(void *context, uint64_t address, size_t size) {
    struct efi_context *ctx = context;
    uefi_call_wrapper(ctx->system->BootServices->FreePages, 2, address, size / 4096);
}
/* Log once at the call site, never inside a map/EBS adapter. */
static void kernel_failure(const struct efi_context *ctx, int prepared) {
    log_text(prepared == -2 ? "ERROR.KERNEL_ADDRESS_UNAVAILABLE\n" : "ERROR.KERNEL_LOAD\n");
    if (prepared == -2) {
        log_text("KERNEL.ALLOCATE_PAGES status="); log_hex(ctx->kernel_status);
        log_text("KERNEL.REQUESTED_BASE="); log_hex(ctx->kernel_requested_base);
        log_text("KERNEL.REQUESTED_SIZE="); log_hex(ctx->kernel_requested_size);
    }
}
static void final_failure(const struct efi_context *ctx, const struct syle_exit_result *r) {
    log_text(r->attempted ? "ERROR.EXIT_BOOT_SERVICES\n" : "ERROR.FINAL_MEMORY_MAP\n");
    log_text("FINAL.STAGE=");
    log_text(ctx->final_stage == FINAL_MAP ? "GetMemoryMap\n" :
             ctx->final_stage == FINAL_ALLOCATE ? "AllocatePool\n" :
             ctx->final_stage == FINAL_EXIT ? "ExitBootServices\n" : "none\n");
    /* SMALL asks the helper to grow; rejection of that request is policy,
     * not a new firmware error. SUCCESS with invalid output is validation. */
    log_text("FINAL.CAUSE=");
    log_text(!EFI_ERROR(ctx->final_status) ||
             (ctx->final_stage == FINAL_MAP && ctx->final_status == EFI_BUFFER_TOO_SMALL)
             ? "transaction-rejected\n" : "firmware-error\n");
    log_text("FINAL.EFI_STATUS="); log_hex(ctx->final_status);
    log_text("FINAL.TRANSACTION_STATUS="); log_hex(r->status);
}
static int platform_launch(void *context, const struct syle_boot_config *config) {
    struct efi_context *ctx = context;
    EFI_BOOT_SERVICES *bs = ctx->system->BootServices;
    EFI_STATUS status;
    boot.mode = config->mode;
#if defined(SMOKE_TEST) || defined(FAULT_TEST) || defined(RING3_TEST)
    log_text("BOOT.TEST_IMAGE_OVERRIDE (compiled diagnostic takes precedence)\n");
#endif
#if defined(SIMPLE_GUEST_PROTOTYPE) || (!defined(SMOKE_TEST) && !defined(FAULT_TEST) && !defined(RING3_TEST))
#ifdef SIMPLE_GUEST_PROTOTYPE
    /* Private transport for this opt-in prototype: V4 payload bytes carry the
     * external guest ELF instead of the management ELF. Default ABI is unchanged. */
    boot.mode = SYLE_BOOT_DIAGNOSTIC;
    {
        const UINTN maximum = SG_MAX_IMAGE;
#else
    if (boot.mode == SYLE_BOOT_NORMAL) {
        const UINTN maximum = 64 * 1024;
#endif
        void *program = NULL;
        status = uefi_call_wrapper(bs->AllocatePool, 3, EfiLoaderData, maximum, &program);
        if (EFI_ERROR(status)) { log_text("ERROR.USER_IMAGE_ALLOC\n"); return -1; }
        UINTN size = 0;
#ifdef SIMPLE_GUEST_PROTOTYPE
        status = read_file(ctx, L"\\syle\\guest.elf", program, maximum, &size);
#else
        status = read_file(ctx, L"\\syle\\mgmt.elf", program, maximum, &size);
#endif
        if (EFI_ERROR(status) || !size) {
#ifdef SIMPLE_GUEST_PROTOTYPE
            log_text("ERROR.SIMPLE_GUEST_IMAGE_READ\n");
#else
            log_text("ERROR.USER_IMAGE_READ\n");
#endif
            uefi_call_wrapper(bs->FreePool, 1, program);
            return -1;
        }
        boot.user_image.base = (UINTN)program;
        boot.user_image.size = size;
        boot.user_image_buffer.base = (UINTN)program;
        boot.user_image_buffer.size = maximum;
#ifdef SIMPLE_GUEST_PROTOTYPE
        log_text("BOOT.SIMPLE_GUEST_IMAGE_READY\n");
#else
        log_text("BOOT.USER_IMAGE_READY\n");
#endif
    }
#endif
    void *kernel_file = NULL;
    UINTN kernel_bytes = 0;
    struct syle_kernel_plan plan;
    status = uefi_call_wrapper(bs->AllocatePool, 3, EfiLoaderData,
                              SYLE_KERNEL_MAX_FILE, &kernel_file);
    if (EFI_ERROR(status)) { log_text("ERROR.KERNEL_FILE_ALLOC\n"); return -1; }
    status = read_file(ctx, L"\\syle\\krnl.elf", kernel_file,
                       SYLE_KERNEL_MAX_FILE, &kernel_bytes);
    if (EFI_ERROR(status) || syle_kernel_elf_inspect(kernel_file, kernel_bytes, &plan)) {
        log_text(EFI_ERROR(status) ? "ERROR.KERNEL_FILE_READ\n" : "ERROR.KERNEL_ELF\n");
        uefi_call_wrapper(bs->FreePool, 1, kernel_file);
        return -1;
    }
    const struct syle_kernel_allocator allocator = {ctx, kernel_reserve, kernel_release};
    int prepared = syle_kernel_prepare(kernel_file, kernel_bytes, &allocator, &plan);
    if (prepared) {
        kernel_failure(ctx, prepared);
        uefi_call_wrapper(bs->FreePool, 1, kernel_file);
        return -1;
    }
    EFI_PHYSICAL_ADDRESS kernel_base = plan.base;
    uefi_call_wrapper(bs->FreePool, 1, kernel_file);
    boot.kernel_image.base = kernel_base;
    boot.kernel_image.size = plan.size;
    boot.kernel_entry = plan.entry;
    log_text("BOOT.KERNEL_LOADED base="); log_hex(kernel_base);
    log_text("BOOT.KERNEL_ENTRY="); log_hex(plan.entry);
#ifdef SMP_TEST
    /* Reserve before the final memory-map/EBS transaction. No firmware AP
     * callback survives EBS; the kernel starts its AP with INIT/SIPI. */
    EFI_PHYSICAL_ADDRESS trampoline=0xfffff;
    status=uefi_call_wrapper(bs->AllocatePages,4,AllocateMaxAddress,EfiLoaderData,1,&trampoline);
    if(EFI_ERROR(status)||!trampoline||trampoline>=0x100000) {
        log_text("ERROR.AP_TRAMPOLINE_ALLOCATION\n");return -1;
    }
    boot.ap_trampoline.base=trampoline;boot.ap_trampoline.size=4096;
#endif
    const struct syle_exit_ops exit_ops = {
        ctx, final_map, final_allocate, final_release, final_exit
    };
    struct syle_exit_result result;
    ctx->final_stage = FINAL_NONE;
    ctx->final_status = EFI_SUCCESS;
    log_text("STAGE0.EXIT_BOOT_SERVICES_BEGIN\n");
    if (!syle_exit_services(&exit_ops, &result)) {
        __asm__ volatile ("cli" ::: "memory");
        boot.map_buffer.base = (UINTN)result.map;
        boot.map_buffer.size = result.capacity;
        boot.memory_map = (UINTN)result.map;
        boot.memory_map_bytes = result.bytes;
        boot.descriptor_size = result.stride;
        boot.descriptor_version = result.version;
        boot.boot_services_exited = 1;
        __asm__ volatile ("mov %%rsp,%0" : "=r"(boot.inherited_rsp));
        __asm__ volatile ("mov %%cr3,%0" : "=r"(boot.inherited_cr3));
        stage0_post_ebs();
    }
    final_failure(ctx, &result);
    if (!result.attempted) return -1;
    /* Firmware can be partially torn down after a failed ExitBootServices.
       Do not return to it or call other Boot Services on this fatal path. */
    __asm__ volatile ("cli" ::: "memory");
    for (;;) __asm__ volatile ("hlt");
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system) {
    EFI_BOOT_SERVICES *bs = system->BootServices;
    EFI_LOADED_IMAGE *loaded = NULL;
    EFI_GUID loaded_guid = LOADED_IMAGE_PROTOCOL;
    EFI_STATUS status;

    serial_init();
    console_init(bs);
    log_text("STAGE0.UEFI_ENTRY\n");
    log_text(console_available() ? "SCREEN.READY\n" : "SCREEN.UNAVAILABLE\n");
    log_text(uart_present ? "UART.READY\n" : "UART.UNAVAILABLE\n");
    status = uefi_call_wrapper(bs->HandleProtocol, 3, image, &loaded_guid, (void **)&loaded);
    if (EFI_ERROR(status)) { log_text("ERROR.LOADED_IMAGE\n"); return status; }
    log_text("IMAGE.BASE="); log_hex((UINTN)loaded->ImageBase);
    log_text("GDB.POST_EBS="); log_hex((UINTN)stage0_post_ebs);
    log_cpu();
    boot.magic = SYLE_BOOT_MAGIC;
    boot.version = SYLE_BOOT_VERSION;
    boot.size = sizeof(boot);
    boot.image.base = (UINTN)loaded->ImageBase;
    boot.image.size = loaded->ImageSize;
    boot.uart_present = uart_present;
    boot.inherited_stack_and_tables = 1;
    console_snapshot(&boot.framebuffer);
    EFI_GUID acpi1 = ACPI_TABLE_GUID, acpi2 = ACPI_20_TABLE_GUID;
    for (UINTN i = 0; i < system->NumberOfTableEntries; ++i) {
        EFI_CONFIGURATION_TABLE *t = &system->ConfigurationTable[i];
        if (CompareGuid(&t->VendorGuid, &acpi2) == 0) {
            boot.acpi_rsdp = (UINTN)t->VendorTable; boot.acpi_revision = 2;
        } else if (boot.acpi_revision != 2 && CompareGuid(&t->VendorGuid, &acpi1) == 0) {
            boot.acpi_rsdp = (UINTN)t->VendorTable; boot.acpi_revision = 1;
        }
    }

    platform_context.image = image;
    platform_context.system = system;
    const struct syle_boot_platform platform = {
        .context = &platform_context, .log = platform_log,
        .read_config = platform_config, .poll_key = platform_key,
        .stall_ms = platform_stall, .launch = platform_launch
    };
    syle_boot_run(&platform);
    log_text("ERROR.BOOT_LAUNCH\n");
    /* Returning here is possible only before the first EBS attempt. */
    if (boot.kernel_image.base) {
        uefi_call_wrapper(bs->FreePages, 2, boot.kernel_image.base, boot.kernel_image.size / 4096);
        boot.kernel_image.base = boot.kernel_image.size = boot.kernel_entry = 0;
    }
    if (boot.user_image_buffer.base) {
        uefi_call_wrapper(bs->FreePool, 1, (void *)(UINTN)boot.user_image_buffer.base);
        boot.user_image_buffer.base = boot.user_image_buffer.size = 0;
        boot.user_image.base = boot.user_image.size = 0;
    }
    return EFI_LOAD_ERROR;
}
