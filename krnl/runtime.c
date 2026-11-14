#include "../include/syle/boot.h"
#include "../include/syle/boot_validate.h"
#include "../efi/src/console.h"
#include "../efi/src/exceptions.h"

/* Independent kernel diagnostics. UART was initialized by EFI; no firmware
 * service or EFI library is called by this separately linked ELF image. */
static int uart_present;
static unsigned output_mode;
void log_output_mode(unsigned mode) { output_mode = mode; }
static inline uint8_t in8(uint16_t port) {
#ifdef SYLE_DIAG_HOST_TEST
    extern uint8_t diag_test_in8(uint16_t);
    return diag_test_in8(port);
#else
    uint8_t value;
    __asm__ volatile("inb %1,%0" : "=a"(value) : "Nd"(port));
    return value;
#endif
}
static inline void out8(uint16_t port, uint8_t value) {
#ifdef SYLE_DIAG_HOST_TEST
    extern void diag_test_out8(uint16_t, uint8_t);
    diag_test_out8(port, value);
#else
    __asm__ volatile("outb %0,%1" :: "a"(value), "Nd"(port));
#endif
}
/* A corrupt boot_info must not suppress every diagnostic. Probe the known
 * legacy debug UART independently without trusting any ABI field. */
static void serial_probe(void) {
    if (in8(0x3fd) == 0xff) return;
    out8(0x3ff, 0x5a);
    if (in8(0x3ff) != 0x5a) return;
    out8(0x3f9, 0); out8(0x3fb, 0x80);
    out8(0x3f8, 1); out8(0x3f9, 0);
    out8(0x3fb, 3); out8(0x3fa, 0xc7); out8(0x3fc, 0x0b);
    uart_present = 1;
}
static void serial_putc(char c) {
    if (!uart_present) return;
    for (unsigned i = 0; i < 10000; ++i) {
        if (in8(0x3fd) & 0x20) { out8(0x3f8, (uint8_t)c); return; }
    }
    uart_present = 0; /* One exhausted polling budget disables the channel. */
}
#ifdef SYLE_HOST_UART_INPUT
#include "host_uart_input.h"
static uint8_t host_uart_read8(void *ctx,uint16_t port) {
    (void)ctx;return in8(port);
}
int host_console_recv(void) {
    return host_uart_poll(uart_present!=0,0,host_uart_read8);
}
#endif
static void log_char(char c) {
    if (output_mode != 2) {
        if (c == '\n') serial_putc('\r');
        serial_putc(c);
    }
    if (output_mode != 1) console_putc(c);
}
void log_text(const char *s) { while (*s) log_char(*s++); }
void log_hex(UINT64 value) {
    static const char digits[] = "0123456789abcdef";
    log_text("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        log_char(digits[(value >> shift) & 15]);
    log_text("\n");
}
__attribute__((noreturn)) void kernel_runtime(const struct syle_boot_info *boot) {
    exceptions_init();
    serial_probe();
    /* boot is trusted loader-owned memory but its version is checked before
     * interpreting framebuffer metadata. The full contract is checked in main. */
    if (!syle_boot_info_valid(boot)) panic("INVALID BOOT INFO");
    uart_present = boot->uart_present == 1;
    console_bind(&boot->framebuffer);
    log_text("KRNL.STANDALONE_ELF\nKRNL.OWN_STARTUP_STACK\n");
    /* Downstream PMM/paging must see the same disabled-screen decision. */
    static struct syle_boot_info normalized;
    normalized = *boot;
    if (!console_available()) normalized.framebuffer = (struct syle_framebuffer){0};
    syle_main(&normalized);
}
