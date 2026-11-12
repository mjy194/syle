#include "smp_cpu.h"
#include <stddef.h>
extern const uint32_t smp_pm_offset,smp_lm_offset;
static bool canonical(uint64_t x) {
    return x<=UINT64_C(0x7fffffffffff)||x>=UINT64_C(0xffff800000000000);
}
static void put(unsigned char *p,uint64_t x,unsigned bytes) {
    for(unsigned i=0;i<bytes;i++)p[i]=(unsigned char)(x>>(8*i));
}
bool smp_bootstrap_prepare(void *page,uint64_t physical,uint64_t cr3,
    uint64_t stack_top,void (*entry)(void *),void *context) {
    size_t size=(size_t)(smp_trampoline_end-smp_trampoline_start);
    uint64_t pc=(uint64_t)(uintptr_t)entry;
    if(!page||((uintptr_t)page&4095)||!physical||(physical&4095)||physical>0xff000||
       !cr3||(cr3&4095)||cr3>=UINT64_C(0x100000000)||
       !pc||pc>=UINT64_C(0x100000000)||!canonical(stack_top)||stack_top<32||
       !canonical(stack_top-32)||!canonical((uint64_t)(uintptr_t)context)||
       size>4096||size<0x260||smp_pm_offset>=size||smp_lm_offset>=size)
        return false;
    unsigned char *p=page;
    for(size_t i=0;i<4096;i++)p[i]=i<size?smp_trampoline_start[i]:0;
    put(p+0x200,cr3,4);
    put(p+0x208,stack_top,8);
    put(p+0x210,pc,8);
    put(p+0x218,(uint64_t)(uintptr_t)context,8);
    put(p+0x222,physical+0x240,4);
    put(p+0x226,physical+smp_pm_offset,4);
    put(p+0x22c,physical+smp_lm_offset,4);
    /* BSP must publish all other config before sending SIPI as well. */
    __atomic_thread_fence(__ATOMIC_RELEASE);
    return true;
}
struct __attribute__((packed)) smp_table_ptr {uint16_t limit;uint64_t base;};
struct __attribute__((packed)) smp_gate {
    uint16_t low,selector;uint8_t ist,flags;uint16_t mid;uint32_t high,reserved;
};
struct __attribute__((packed)) smp_tss {
    uint32_t reserved0;
    uint64_t rsp[3],reserved1,ist[7],reserved2;
    uint16_t reserved3,iomap;
};
_Static_assert(sizeof(struct smp_tss)==104,"x86-64 TSS size");
static struct smp_gate ap_idt[256] __attribute__((aligned(16)));
static uint64_t ap_gdt[5] __attribute__((aligned(16)));
static struct smp_tss ap_tss;
static unsigned char ap_ist[32768] __attribute__((aligned(16)));
static unsigned char ap_df_ist[16384] __attribute__((aligned(16)));
static unsigned char ap_nmi_ist[16384] __attribute__((aligned(16)));
static struct smp_cpu_fault *ap_fault;
static unsigned char ap_ipi_ist[16384] __attribute__((aligned(16)));
static void (*ap_ipi_handler)(void *);
static void *ap_ipi_context;
extern void smp_ipi_entry(void);
extern const uintptr_t smp_exception_stubs[33];
extern void smp_load_gdt(const struct smp_table_ptr *);
void smp_cpu_init(struct smp_cpu_fault *fault) {
    __asm__ volatile("cli":::"memory");
    ap_fault=fault;
    /* Dedicated single-AP descriptor; LTR busy-bit cannot touch BSP GDT. */
    ap_tss=(struct smp_tss){0};
    ap_tss.ist[0]=(uintptr_t)(ap_ist+sizeof ap_ist);
    ap_tss.ist[1]=(uintptr_t)(ap_df_ist+sizeof ap_df_ist);
    ap_tss.ist[2]=(uintptr_t)(ap_nmi_ist+sizeof ap_nmi_ist);
    ap_tss.ist[3]=(uintptr_t)(ap_ipi_ist+sizeof ap_ipi_ist);
    ap_tss.iomap=sizeof ap_tss;
    uint64_t base=(uintptr_t)&ap_tss,limit=sizeof ap_tss-1;
    ap_gdt[0]=0;
    ap_gdt[1]=UINT64_C(0x00af9b000000ffff);
    ap_gdt[2]=UINT64_C(0x00cf93000000ffff);
    ap_gdt[3]=limit|((base&0xffffff)<<16)|(UINT64_C(0x89)<<40)|(((base>>24)&255)<<56);
    ap_gdt[4]=base>>32;
    for(unsigned i=0;i<256;i++) {
        uint64_t a=smp_exception_stubs[i<32?i:32];
        ap_idt[i]=(struct smp_gate){a&0xffff,8,i==8?2:i==2?3:1,0x8e,
                                  (a>>16)&0xffff,a>>32,0};
    }
    struct smp_table_ptr gdtr={sizeof ap_gdt-1,(uintptr_t)ap_gdt};
    struct smp_table_ptr idtr={sizeof ap_idt-1,(uintptr_t)ap_idt};
    smp_load_gdt(&gdtr);
    uint16_t tr=0x18;
    __asm__ volatile("ltr %0"::"r"(tr):"memory");
    __asm__ volatile("lidt %0"::"m"(idtr):"memory");
}
__attribute__((noreturn)) void smp_cpu_fault_publish(uint64_t vector,uint64_t error,
    uint64_t rip,uint64_t cr2) {
    /* No firmware, console, locks or allocation. Never return to bad state. */
    if(ap_fault) {
        ap_fault->vector=vector;ap_fault->error=error;
        ap_fault->rip=rip;ap_fault->cr2=cr2;
        __atomic_store_n(&ap_fault->valid,1,__ATOMIC_RELEASE);
    }
    for(;;)__asm__ volatile("cli; hlt":::"memory");
}

void smp_cpu_set_ipi(void (*handler)(void *),void *context) {
    /* IF=0 and single AP makes descriptor publication nonconcurrent with
     * maskable delivery. Unknown vectors retain the fail-closed gates. */
    ap_ipi_context=context;
    ap_ipi_handler=handler;
    __asm__ volatile("":::"memory");
    uint64_t a=(uintptr_t)smp_ipi_entry;
    ap_idt[0x40]=(struct smp_gate){a&0xffff,8,4,0x8e,(a>>16)&0xffff,a>>32,0};
    __asm__ volatile("":::"memory");
}
void smp_cpu_ipi_dispatch(void) {
    if(!ap_ipi_handler) smp_cpu_fault_publish(0x40,0,0,0);
    ap_ipi_handler(ap_ipi_context);
}
