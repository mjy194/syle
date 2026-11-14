#include "exceptions.h"
#include "console.h"

struct __attribute__((packed)) table_ptr { UINT16 limit; UINT64 base; };
struct __attribute__((packed)) gate {
    UINT16 offset_low, selector;
    UINT8 ist, flags;
    UINT16 offset_mid;
    UINT32 offset_high, reserved;
};
struct __attribute__((packed)) tss64 {
    UINT32 reserved0;
    UINT64 rsp[3], reserved1, ist[7], reserved2;
    UINT16 reserved3, iomap;
};
static struct gate idt[256] __attribute__((aligned(16)));
static UINT64 gdt[7] __attribute__((aligned(16)));
struct tss64 tss;
static UINT8 exception_stack[32768] __attribute__((aligned(16)));
static UINT8 double_fault_stack[16384] __attribute__((aligned(16)));
static UINT8 nmi_stack[16384] __attribute__((aligned(16)));
static volatile unsigned panicking;
extern void *exception_stubs[256];
extern void load_gdt_segments(const struct table_ptr *);

void exceptions_init(void) {
    __asm__ volatile("cli; cld" ::: "memory");
    UINT64 base = (UINTN)&tss;
    UINT64 limit = sizeof(tss) - 1;
    tss.ist[0] = (UINTN)(exception_stack + sizeof(exception_stack));
    tss.ist[1] = (UINTN)(double_fault_stack + sizeof(double_fault_stack));
    tss.ist[2] = (UINTN)(nmi_stack + sizeof(nmi_stack));
    tss.iomap = sizeof(tss);
    gdt[1] = 0x00af9a000000ffffULL; /* ring 0, long-mode code */
    gdt[2] = 0x00cf92000000ffffULL; /* ring 0 data */
    gdt[3] = limit | ((base & 0xffffff) << 16) | (0x89ULL << 40)
           | (((base >> 24) & 255) << 56);
    gdt[4] = base >> 32;
    struct table_ptr gdtr = {sizeof(gdt)-1, (UINTN)gdt};
    load_gdt_segments(&gdtr);
    UINT16 selector = 0x18;
    __asm__ volatile("ltr %0" : : "r"(selector));
    for (unsigned i = 0; i < 256; ++i) {
        UINT64 address = (UINTN)exception_stubs[i];
        idt[i] = (struct gate){address & 0xffff, 8, i == 8 ? 2 : i == 2 ? 3 : 1, 0x8e,
                              (address >> 16) & 0xffff, address >> 32, 0};
    }
    struct table_ptr idtr = {sizeof(idt)-1, (UINTN)idt};
    __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
}

void exceptions_user_setup(UINT64 stack_top, void (*entry)(void)) {
    tss.rsp[0] = stack_top;
    gdt[5] = 0x00affa000000ffffULL; /* DPL3 long code, selector 0x2b */
    gdt[6] = 0x00cff2000000ffffULL; /* DPL3 data, selector 0x33 */
    UINT64 address = (UINTN)entry;
    idt[128] = (struct gate){address & 0xffff, 8, 0, 0xee,
                            (address >> 16) & 0xffff, address >> 32, 0};
}

static __attribute__((noreturn)) void halt(void) {
    extern void exception_stop(void) __attribute__((noreturn));
    exception_stop();
}
static void begin_panic(void) {
    __asm__ volatile("cli" ::: "memory");
    if (__atomic_exchange_n(&panicking, 1, __ATOMIC_RELAXED)) halt();
    /* Complete the UART pass before touching potentially broken MMIO. */
    log_output_mode(1);
}
__attribute__((noreturn)) void panic(const char *message) {
    begin_panic();
    for (unsigned pass = 0; pass < 2; ++pass) {
        log_output_mode(pass ? 2 : 1);
        if (pass) console_panic_style();
        log_text("PANIC: CPU HALTED\n");
        log_text(message);
        log_text("\nPANIC.COMPLETE\n");
    }
    halt();
}
struct frame {
    UINT64 r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rdx,rcx,rbx,rax;
    UINT64 vector,error,rip,cs,rflags,rsp,ss;
};
extern void ring3_fault(const struct frame *, UINT64) __attribute__((weak));
#define FIELD(name) do { log_text(#name "="); log_hex(f->name); } while (0)
__attribute__((noreturn)) void exception_panic(const struct frame *f) {
    UINT64 cr2;
    __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    log_output_mode(1);
    if (f->vector != 2 && ring3_fault) ring3_fault(f, cr2);
    begin_panic();
    for (unsigned pass = 0; pass < 2; ++pass) {
    log_output_mode(pass ? 2 : 1);
    if (pass) console_panic_style();
    log_text("PANIC: CPU HALTED\n");
    log_text(f->vector < 32 ? "REAL CPU EXCEPTION\n" : "UNHANDLED VECTOR (SAFE FALLBACK)\n");
    FIELD(vector); FIELD(error); FIELD(rip); FIELD(cs); FIELD(rflags);
    FIELD(rsp); FIELD(ss);
    log_text("cr2="); log_hex(cr2);
    FIELD(rax); FIELD(rbx); FIELD(rcx); FIELD(rdx); FIELD(rsi); FIELD(rdi);
    FIELD(rbp); FIELD(r8); FIELD(r9); FIELD(r10); FIELD(r11); FIELD(r12);
    FIELD(r13); FIELD(r14); FIELD(r15);
    log_text("PANIC.COMPLETE\n");
    }
    halt();
}
