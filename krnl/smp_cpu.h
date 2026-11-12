#ifndef SYLE_SMP_CPU_H
#define SYLE_SMP_CPU_H
#include <stdint.h>
#include <stdbool.h>
/* Single diagnostic AP only. BSP initializes valid=0 before startup and
 * reads fields only after acquire-loading valid. Fault handling never logs. */
struct smp_cpu_fault {
    uint64_t vector,error,rip,cr2;
    uint32_t valid;
};
extern const unsigned char smp_trampoline_start[],smp_trampoline_end[];
/* BSP-only, before SIPI. Fresh private 4K page and stack; no reuse while AP
 * may execute. Physical low page must be identity mapped by supplied CR3.
 * CR3 is a <4GiB page base; page/stack/C entry/context/private CPU tables must
 * be mapped in this page table. Caller gates long mode, PAE and NX support.
 * Entry and context follow SysV x86-64 ABI; entry must not return. */
bool smp_bootstrap_prepare(void *page,uint64_t physical,uint64_t cr3,
    uint64_t stack_top,void (*entry)(void *),void *context);
/* Called on AP, IF=0, exactly once, before any diagnostic work. Installs
 * private GDT/TSS/IST/IDT; no firmware or BSP exception-state dependency.
 * The tiny pre-init transition window has no general exception recovery. */
void smp_cpu_init(struct smp_cpu_fault *fault);
/* AP-only, after init and while IF=0. Install once before interrupts are
 * enabled; caller must not replace while a callback can run. Interrupt gate
 * clears IF; callback must not enable it. Callback owns APIC EOI, if needed.
 * Callback must use kernel general-register-only/no-red-zone ABI. */
void smp_cpu_set_ipi(void (*handler)(void *),void *context);
#endif
