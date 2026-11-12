#include "linux_guest_dispatch.h"
#include "linux_guest_manager.h"
#ifndef MG_FIXED_INPUTS_VERIFIED
#error "Managed guest requires pinned-image hash gate before compiling startup"
#endif
#ifdef MG_EXPERIMENTAL_APIC
#include "linux_profile_service.h"
static struct lps_context profile;
static bool synthetic_time(void*x,uint64_t*t){(void)x;uint32_t a,d;
 __asm__ volatile("lfence;rdtsc":"=a"(a),"=d"(d)::"memory");*t=((uint64_t)d<<32)|a;return true;}
#else
static struct lgd_context dispatcher;
#endif
static struct lgm_service_ops service;
/* Explicit ownership of logical UART level only. NOT a LAPIC injection sink.
 * Guest CPUID hides APIC; this profile is polled UART until a routed IRQ backend
 * is separately installed and verified. Do not label true as guest delivery. */
static bool uart_irq4_level;
static bool retain_level(void *ctx,unsigned irq,bool level) {
 (void)ctx;if(irq!=4)return false;uart_irq4_level=level;return true;
}
static void cpu(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
 __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
bool linux_guest_startup_install(void) {
 struct lgd_config c={.max_exits=1u<<20,.fixed_input_verified=true,.sync_irq=retain_level};
 uint32_t a,b,x,d;
 cpu(0,&c.max_basic,&c.vendor_b,&c.vendor_c,&c.vendor_d);
 cpu(0x80000000,&c.max_extended,&b,&x,&d);
 if(c.max_basic<1||c.max_extended<0x80000008)return false;
 cpu(1,&a,&b,&x,&c.cpu.basic_edx);
 cpu(0x80000001,&a,&b,&x,&c.cpu.extended_edx);
 cpu(0x80000008,&a,&b,&x,&d);c.cpu.physical_bits=a&255;
#ifdef MG_EXPERIMENTAL_APIC
 return lps_init(&profile,&c,0,synthetic_time,&service)&&linux_guest_manager_install(&service);
#else
 return lgd_init(&dispatcher,&c,&service)&&linux_guest_manager_install(&service);
#endif
}
