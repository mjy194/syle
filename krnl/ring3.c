#include "ring3.h"
#include "../include/syle/host_console.h"
#ifdef SYLE_HOST_UART_INPUT
extern int host_console_recv(void);
#endif
#include "guest_control.h"
#include "paging.h"
#include "memory.h"
#include "user_elf.h"
#include "../efi/src/exceptions.h"

/* Bounded bootstrap process. Inherit kernel-owned supervisor mappings;
 * user pages live in a separate PML4 slot. Storage belongs to krnl. */
#define PAGE 4096u
#define USER_BASE UINT64_C(0x8000000000)
#define NX (UINT64_C(1)<<63)
static uint64_t root[512] __attribute__((aligned(PAGE)));
static uint64_t user_pdpt[512] __attribute__((aligned(PAGE)));
static uint64_t user_pd[512] __attribute__((aligned(PAGE)));
static uint64_t user_pt[512] __attribute__((aligned(PAGE)));
static unsigned char code[PAGE] __attribute__((aligned(PAGE)));
static unsigned char data[PAGE] __attribute__((aligned(PAGE)));
static unsigned char stack[PAGE] __attribute__((aligned(PAGE)));
static unsigned char kernel_stack[32768] __attribute__((aligned(16)));
static unsigned char syscall_stack[32768] __attribute__((aligned(16)));
#ifdef RING3_CASE
#define SYLE_RING3_DIAGNOSTIC
extern unsigned char mgmt_image_start[], mgmt_image_end[];
#endif
static uint64_t user_entry = USER_BASE;
extern void ring3_syscall_entry(void);
extern __attribute__((noreturn)) void ring3_kernel_switch(uint64_t, uint64_t, void (*)(void));
extern __attribute__((noreturn)) void ring3_enter(uint64_t, uint64_t, uint64_t, uint64_t);
extern void exceptions_user_setup(uint64_t stack_top, void (*entry)(void));
#ifndef RING3_CASE
#define RING3_CASE 0
#endif
/* One trusted bootstrap manager. Owner is never supplied by user registers.
 * A real guest backend must be registered by boot code before ring3_start.
 * Without it guest calls return ENOTSUP, never simulated success. */
static struct guest_control managed_guest = { .manager=1 };
static bool manager_started, backend_registered;
bool ring3_guest_bind(const struct guest_backend *backend, void *context) {
    if (manager_started || backend_registered || !backend ||
        !backend->prepare || !backend->start || !backend->stop || !backend->release)
        return false;
    guest_control_init(&managed_guest,1,backend,context);
    backend_registered=true;
    return true;
}
static void guest_owner_exit(void) {
    if (guest_control_owner_exit(&managed_guest,1))
        panic("GUEST CLEANUP QUARANTINED");
}
static unsigned syscall_count;
static uint64_t fault_probe_address;
struct frame {
    uint64_t r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rdx,rcx,rbx,rax;
    uint64_t vector,error,rip,cs,rflags,rsp,ss;
};
#ifdef SYLE_RING3_DIAGNOSTIC
static __attribute__((noreturn)) void finish(int pass) {
    log_text(pass ? "RING3.PASS\n" : "RING3.FAIL\n");
    __asm__ volatile("outl %0,%1" :: "a"(pass ? 0x10u : 0x11u), "Nd"((uint16_t)0xf4));
    for (;;) __asm__ volatile("cli; hlt");
}
#endif
/* Only these complete user ranges are readable; subtraction avoids overflow.
 * No user mappings may change while a copy is in progress in this single CPU
 * prototype. Strings are copied/printed as bounded bytes, never kernel strings.
 */
static int readable(uint64_t p, uint64_t n) {
    if (n > 256) return 0;
    if (p >= USER_BASE && p < USER_BASE + 2*PAGE)
        return n <= USER_BASE + 2*PAGE - p;
    if (p >= USER_BASE + 3*PAGE && p < USER_BASE + 4*PAGE)
        return n <= USER_BASE + 4*PAGE - p;
    return 0;
}
void ring3_syscall(struct frame *f) {
    if (f->cs != 0x2b || f->ss != 0x33 || (f->rflags & 0x3000))
        panic("BAD USER SYSCALL FRAME");
    ++syscall_count;
    switch (f->rax) {
    case SYLE_GUEST_CREATE: case SYLE_GUEST_START: case SYLE_GUEST_STOP:
    case SYLE_GUEST_STATUS: case SYLE_GUEST_DESTROY: case SYLE_GUEST_REAP:
    case SYLE_GUEST_STEP: case SYLE_GUEST_CONSOLE_SEND: case SYLE_GUEST_CONSOLE_RECV:
    case SYLE_GUEST_CONSOLE_SPACE:
        f->rax=(uint64_t)guest_control_call(&managed_guest,1,f->rax,f->rdi,f->rsi);
        break;
#ifdef SYLE_HOST_UART_INPUT
    case SYLE_HOST_CONSOLE_RECV:
        f->rax=(uint64_t)(int64_t)((f->rdi||f->rsi)?SYLE_HOST_CONSOLE_INVALID:host_console_recv());
        break;
#endif
    case 1:
        if (!readable(f->rdi, f->rsi)) { f->rax = UINT64_MAX; break; }
        for (uint64_t i = 0; i < f->rsi; ++i) {
            char s[2] = {((const char *)(uintptr_t)f->rdi)[i], 0};
            log_text(s);
        }
        f->rax = f->rsi;
        break;
    case 2: f->rax = 1; break; /* one online bootstrap CPU; no VM claims */
    case 3:
        guest_owner_exit();
        log_text("RING3.EXIT syscall_count="); log_hex(syscall_count);
#ifdef SYLE_RING3_DIAGNOSTIC
        finish(RING3_CASE == 0 && f->rdi == 0 && syscall_count == 4);
#else
        log_text("MGMT.EXIT status="); log_hex(f->rdi);
        log_text("KRNL.IDLE\n");
        for (;;) __asm__ volatile("cli; hlt");
#endif
    default: f->rax = UINT64_MAX; break;
    }
}
/* Shared exception hook. Diagnostics assert CPU isolation; the normal
 * bootstrap currently halts on a user fault instead of recovering a process. */
void ring3_fault(const struct frame *f, uint64_t cr2) {
    if (f->vector == 2 || (f->cs & 3) != 3) return;
    guest_owner_exit();
    log_text("RING3.USER_FAULT vector="); log_hex(f->vector);
    log_text("RING3.USER_FAULT error="); log_hex(f->error);
    (void)cr2;
#ifdef SYLE_RING3_DIAGNOSTIC
    int pass = 0;
    if (RING3_CASE == 1) pass = f->vector == 14 && f->error == 5 && cr2 == fault_probe_address;
    if (RING3_CASE == 2) pass = f->vector == 14 && f->error == 7 && cr2 == USER_BASE;
    if (RING3_CASE == 3) pass = f->vector == 14 && f->error == 21 && cr2 == USER_BASE+4*PAGE-24;
    if (RING3_CASE == 4) pass = f->vector == 13 && f->error == 0;
    finish(pass);
#else
    panic("MANAGEMENT PROCESS FAULT");
#endif
}
static __attribute__((noreturn)) void runtime(void) {
#if defined(WX_ALIAS_TEST) && WX_ALIAS_TEST != 3
    /* Code copy above has populated the old writable identity translation.
     * The RX user alias now exists in the active root. Both addresses must
     * reject a supervisor write, proving seal + TLB revocation in hardware. */
    uint64_t alias = WX_ALIAS_TEST == 1 ? (uintptr_t)code : USER_BASE;
    log_text("WX.ALIAS_WRITE_TEST address="); log_hex(alias);
    *(volatile unsigned char *)(uintptr_t)alias = 0x90;
    panic("WX ALIAS WRITE WAS ALLOWED");
#endif
    exceptions_user_setup((uintptr_t)(syscall_stack + sizeof syscall_stack), ring3_syscall_entry);
    log_text("RING3.OWN_CR3_AND_STACK\nRING3.USER_RX_DATA_RW_NX\n");
    fault_probe_address=(uintptr_t)root;
    uint64_t high=pmm_alloc_page_above(UINT64_C(0x100000000));
    if (high) {
        volatile uint64_t *v=paging_phys_to_virt(high);
        *v=UINT64_C(0x1234fedcba987650);
        if (*v!=UINT64_C(0x1234fedcba987650)) panic("RING3 HIGH MAP LOST");
        *v=0;
        if (!pmm_free_page(high)) panic("RING3 HIGH FREE FAILED");
        log_text("RING3.HIGH_SUPERVISOR_MAPPING_PASS\n");
        if (RING3_CASE==1) {
            fault_probe_address=(uintptr_t)v;
            log_text("RING3.HIGH_USER_DENIAL_TEST\n");
        }
    }
    /* Assembly-only entry, no synthetic return address. */
    ring3_enter(user_entry, USER_BASE+4*PAGE-16, RING3_CASE, fault_probe_address);
}
__attribute__((noreturn)) void ring3_start(const struct syle_boot_info *boot) {
    manager_started=true;
    if (!boot || boot->image.base >= UINT64_C(0x100000000) ||
        boot->image.size > UINT64_C(0x100000000)-boot->image.base ||
        (boot->framebuffer.valid && (boot->framebuffer.base >= UINT64_C(0x100000000) ||
         boot->framebuffer.size > UINT64_C(0x100000000)-boot->framebuffer.base)))
        panic("RING3 BOOTSTRAP REQUIRES IMAGE/FB BELOW 4G");
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(0x80000000u));
    if (a < 0x80000001u) panic("RING3 NX REQUIRED");
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(0x80000001u));
    if (!(d & (1u<<20))) panic("RING3 NX REQUIRED");
    __asm__ volatile("rdmsr" : "=a"(a),"=d"(d) : "c"(0xc0000080u));
    a = (a | (1u<<11)) & ~1u; /* NXE on; unsupported SYSCALL disabled */
    __asm__ volatile("wrmsr" :: "a"(a),"d"(d),"c"(0xc0000080u));
    __asm__ volatile("wrmsr" :: "a"(0u),"d"(0u),"c"(0x174u)); /* SYSENTER_CS */
    uintptr_t size;
#ifdef SYLE_RING3_DIAGNOSTIC
    size = (uintptr_t)mgmt_image_end - (uintptr_t)mgmt_image_start;
    if (!size || size > PAGE) panic("INVALID MANAGEMENT FLAT IMAGE");
    for (uintptr_t i=0; i<size; ++i) code[i]=mgmt_image_start[i];
#else
    if (!boot->user_image.base || !boot->user_image.size ||
        boot->user_image_buffer.base != boot->user_image.base ||
        boot->user_image.size > boot->user_image_buffer.size ||
        boot->user_image.base >= UINT64_C(0x100000000) ||
        boot->user_image.size > UINT64_C(0x100000000)-boot->user_image.base)
        panic("INVALID MANAGEMENT ELF BUFFER");
    size = boot->user_image.size;
    if (!user_elf_load((const void *)(uintptr_t)boot->user_image.base,
                      size, code, data, &user_entry))
        panic("INVALID MANAGEMENT ELF");
#endif
    /* Finish loading before removing ALL supervisor writable aliases. The
     * full BSP TLB flush completes before publishing the user RX mapping. */
    if (!paging_seal_code_page((uintptr_t)code))
        panic("USER CODE ALIAS SEAL FAILED");
    log_text("RING3.ALL_CODE_ALIASES_RO\n");
#if defined(WX_ALIAS_TEST) && WX_ALIAS_TEST == 3
    /* No subsequent root switch: isolate the seal's own TLB invalidation. */
    log_text("WX.ALIAS_WRITE_TEST address="); log_hex((uintptr_t)code);
    *(volatile unsigned char *)code = 0x90;
    panic("WX ALIAS WRITE WAS ALLOWED");
#endif
    /* Preserve the early kernel mappings and W^X permissions. */
    root[0] = ((uint64_t *)(uintptr_t)paging_root())[0];
    for (unsigned i=256;i<512;++i)
        root[i] = ((uint64_t *)(uintptr_t)paging_root())[i];
    root[1] = (uintptr_t)user_pdpt | 7;
    user_pdpt[0] = (uintptr_t)user_pd | 7;
    user_pd[0] = (uintptr_t)user_pt | 7;
    user_pt[0] = (uintptr_t)code | 5;       /* user RX */
    user_pt[1] = (uintptr_t)data | 7 | NX; /* user RW, non-executable */
    user_pt[3] = (uintptr_t)stack | 7 | NX;/* page 2 is an unmapped guard */
    uint64_t cr4;
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    if (cr4 & ((1u<<12) | (1u<<21)))
        panic("RING3 BOOTSTRAP DOES NOT SUPPORT LA57/SMAP");
    /* Flush inherited global translations too; this root does not use PGE. */
    cr4 &= ~(1u<<7);
    __asm__ volatile("mov %0,%%cr4" :: "r"(cr4) : "memory");
    uint64_t cr0;
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    cr0 |= 1u<<16; /* supervisor write-protect */
    __asm__ volatile("mov %0,%%cr0" :: "r"(cr0) : "memory");
#ifdef SYLE_RING3_DIAGNOSTIC
    log_text("RING3.FLAT_IMAGE_LOADED bytes=");
#else
    log_text("MGMT.ELF_LOADED bytes=");
#endif
    log_hex(size);
    ring3_kernel_switch((uintptr_t)root, (uintptr_t)(kernel_stack+sizeof kernel_stack), runtime);
}
