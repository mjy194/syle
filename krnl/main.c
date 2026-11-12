#include <stdbool.h>
#include "../include/syle/boot.h"
#include "../include/syle/boot_validate.h"
#include "../efi/src/exceptions.h"

#include "ring3.h"
#include "memory.h"
#include "paging.h"
#include "vmx.h"
#ifdef SIMPLE_GUEST_PROTOTYPE
#include "simple_guest.h"
#endif
#ifdef SMP_TEST
#include "smp.h"
#endif

/* First kernel entry, loaded from its own ELF image. No firmware calls. */
__attribute__((noreturn)) void syle_main(const struct syle_boot_info *boot) {
    __asm__ volatile ("cli" ::: "memory");
    if (!syle_boot_info_valid(boot))
        panic("INVALID BOOT INFO");
    log_text("SYLE.KRNL_ENTRY\nBOOT_INFO.V4\n");
    log_text("BOOT.MAP_BYTES="); log_hex(boot->memory_map_bytes);
    log_text("BOOT.IMAGE_BASE="); log_hex(boot->image.base);
    log_text("BOOT.KERNEL_BASE="); log_hex(boot->kernel_image.base);
    log_text("BOOT.ACPI_RSDP="); log_hex(boot->acpi_rsdp);
    log_text("BOOT.NO_RECLAIM (firmware reservations retained)\n");
    log_text("STAGE0.POST_EBS\n");
    log_text("IDT+IST READY\n");
    if (!pmm_init(boot)) panic("INVALID PHYSICAL MEMORY MAP");
    log_text("KRNL.PMM_READY\n");
    uint64_t probe = pmm_alloc_page();
    if (!probe || !pmm_free_page(probe) || pmm_free_page(probe))
        panic("PMM ALLOC/FREE SELFTEST FAILED");
    paging_init(boot);
#if defined(LINUX_FIRST_EXIT_RECOVERY) || defined(LINUX_CR_RECOVERY) || defined(LINUX_PF_RECOVERY)
#if defined(LINUX_PF_RECOVERY)
    extern bool linux_recovery_pf_probe(void);
    bool observed = linux_recovery_pf_probe();
#elif defined(LINUX_CR_RECOVERY)
    extern bool linux_recovery_cr_probe(void);
    bool observed = linux_recovery_cr_probe();
#else
    extern bool linux_recovery_first_exit(void);
    bool observed = linux_recovery_first_exit();
#endif
    __asm__ volatile("outl %0,%1" : : "a"(observed ? 0x10u : 0x11u), "Nd"((UINT16)0xf4));
    for (;;) __asm__ volatile("cli; hlt");
#endif
#ifdef SIMPLE_GUEST_PROTOTYPE
    /* The loader-owned file stays reserved by the existing V4 payload ranges.
     * A separate opt-in target is the only consumer of this provisional ABI. */
    if (boot->user_image.size > SG_MAX_IMAGE ||
        !simple_guest_bind(paging_phys_to_virt(boot->user_image.base),
                           (size_t)boot->user_image.size)) {
        log_text("SIMPLE_GUEST.IMAGE_REJECTED no_vmx_attempt\n");
    } else {
        vmx_probe();
    }
    bool completed = simple_guest_succeeded();
    log_text(completed ? "SIMPLE_GUEST.PROTOTYPE_PASS\n" :
                         "SIMPLE_GUEST.NOT_COMPLETED\n");
    /* This image is a QEMU-targeted prototype, never the default boot image.
     * Missing VMX and all rejected/failed paths use failure status, not smoke. */
    __asm__ volatile ("outl %0,%1" : : "a"(completed ? 0x10u : 0x11u),
                      "Nd"((UINT16)0xf4));
    for (;;) __asm__ volatile("cli; hlt");
#endif
#ifdef NESTED_WT_PLATFORM_TEST
    extern bool cache_registered_platform_type(const struct syle_boot_info *,uint8_t);
    if(cache_registered_platform_type(boot,4))vmx_probe();
    else log_text("NESTED.WT_PLATFORM_BLOCKED no_transition\n");
#elif defined(NESTED_UC_PLATFORM_TEST)
    extern bool cache_registered_platform(const struct syle_boot_info *);
    if(cache_registered_platform(boot))vmx_probe();
    else log_text("NESTED.UC_PLATFORM_BLOCKED no_transition\n");
#elif defined(CACHE_UC_TEST)
    extern void cache_uc_probe(const struct syle_boot_info *);
    cache_uc_probe(boot);
#elif defined(SMP_TEST)
    smp_probe(boot);
#else
    vmx_probe();
#endif
    /* Verify that high frames are actually writable through the new mapping,
     * not merely counted by the allocator. No requirement on small machines. */
    uint64_t high=pmm_alloc_page_above(UINT64_C(0x100000000));
    if (high) {
        volatile uint64_t *v=paging_phys_to_virt(high);
        for (unsigned i=0;i<512;++i) v[i]=high^i^UINT64_C(0x5a6bd38ef091c247);
        for (unsigned i=0;i<512;++i)
            if (v[i]!=(high^i^UINT64_C(0x5a6bd38ef091c247))) panic("HIGH RAM SELFTEST FAILED");
        for (unsigned i=0;i<512;++i) v[i]=0;
        if (!pmm_free_page(high)) panic("HIGH RAM FREE FAILED");
        log_text("PMM.HIGH_WRITE_READ_FREE_PASS physical=");log_hex(high);
    } else log_text("PMM.NO_FREE_HIGH_RAM\n");
    log_text("PMM.ADDRESS_LIMIT=");log_hex(pmm_address_limit());
    struct pmm_stats stats = pmm_get_stats();
    log_text("PMM.FREE_PAGES="); log_hex(stats.free_pages);
    log_text("PMM.ALLOCATED_PAGES="); log_hex(stats.allocated_pages);
#ifdef MEMORY_CASE
    log_text("MEMORY.PROTECTION_TEST\n");
#if MEMORY_CASE == 1
    *(volatile unsigned char *)(uintptr_t)syle_main = 0x90;
#elif MEMORY_CASE == 2
    unsigned char instructions[16] = {0xc3};
    ((void (*)(void))(uintptr_t)instructions)();
#elif MEMORY_CASE == 3
    uint64_t zero = 0;
    __asm__ volatile("mov (%0),%%rax"::"r"(zero):"rax","memory");
#elif MEMORY_CASE == 4
    extern char __rodata_start[];
    *(volatile unsigned char *)__rodata_start = 0x90;
#elif MEMORY_CASE == 5
    static unsigned char nx_data[4096] __attribute__((aligned(4096))) = {0xc3};
    ((void (*)(void))(uintptr_t)nx_data)();
#endif
    panic("MEMORY PROTECTION FAILED");
#endif
#ifdef RING3_TEST
    ring3_start(boot);
#endif
#ifdef FAULT_TEST
#if FAULT_TEST == 6
    __asm__ volatile("ud2");
#elif FAULT_TEST == 14
    *(volatile UINT64 *)0xffff800000000000ULL = 0x1234;
#else
    panic("EXPLICIT PANIC TEST");
#endif
#endif
#ifdef SMOKE_TEST
    log_text("STAGE0.SMOKE_PASS\n");
    /* QEMU isa-debug-exit: (0x10 << 1) | 1 == 33. */
    __asm__ volatile ("outl %0,%1" : : "a"(0x10u), "Nd"((UINT16)0xf4));
#endif
#if !defined(RING3_TEST) && !defined(FAULT_TEST) && !defined(SMOKE_TEST)
    if (boot->mode == 0) {
#ifdef SYLE_MANAGED_GUEST
        extern bool linux_guest_startup_install(void);
        if (!linux_guest_startup_install()) panic("MANAGED GUEST BACKEND UNAVAILABLE");
#endif
        ring3_start(boot);
    }
    log_text("KRNL.DIAGNOSTIC_IDLE\n");
#endif
    for (;;) __asm__ volatile ("hlt");
}

