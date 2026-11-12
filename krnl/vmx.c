#include "vmx.h"
#ifdef VMX_NESTED_VCPU_TEST
#include "nested_vcpu.h"
#if !defined(VMX_ROOT_TEST) || !defined(VMX_GUEST_TEST) || !defined(VMX_EPT_TEST) || !defined(VMX_NESTED_TEST)
#error "nested vCPU context diagnostic requires root, guest, EPT and nested execution"
#endif
#endif
#include "memory.h"
#include "paging.h"
#include "vmx_memory.h"
#ifdef VMX_EPT_TEST
#include "ept.h"
#endif
#ifdef VMX_GUEST_TEST
#include "vmx_guest.h"
#endif
#include "../efi/src/exceptions.h"
static uint64_t rdmsr(uint32_t n) {
    uint32_t a,d;
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));
    return ((uint64_t)d<<32)|a;
}
void vmx_probe(void) {
    uint32_t a=1,b,c,d;
    __asm__ volatile("cpuid":"+a"(a),"=b"(b),"=c"(c),"=d"(d));
    if (!(c & (1u<<5))) {
        log_text("VMX.UNAVAILABLE CPUID.VMX=0; no VMX MSRs/instructions executed\n");
        return;
    }
    struct vmx_caps caps={0};
    caps.feature_control=rdmsr(0x3a);
    caps.basic=rdmsr(0x480);
    caps.cr0_fixed0=rdmsr(0x486); caps.cr0_fixed1=rdmsr(0x487);
    caps.cr4_fixed0=rdmsr(0x488); caps.cr4_fixed1=rdmsr(0x489);
    __asm__ volatile("mov %%cr0,%0":"=r"(caps.cr0));
    __asm__ volatile("mov %%cr4,%0":"=r"(caps.cr4));
    log_text("VMX.FEATURE_CONTROL=");log_hex(caps.feature_control);
    log_text("VMX.BASIC=");log_hex(caps.basic);
    const char *reason=vmx_gate(&caps);
    if (reason) { log_text("VMX.BLOCKED ");log_text(reason);log_text("\n");return; }
    log_text("VMX.CAPABILITY_GATE_PASS (not VMX execution)\n");
#ifndef VMX_ROOT_TEST
    log_text("VMX.ROOT_TEST_DISABLED\n");
#else
    /* Explicit diagnostic build only. Never write IA32_FEATURE_CONTROL.
     * Allocator admits WB RAM only. Region uses required revision and alignment.
     * CPU may restrict VMX control structures to low 4 GiB via BASIC[48]. */
    uint64_t limit=(caps.basic & (1ULL<<48)) ? (1ULL<<32) : pmm_address_limit();
    uint64_t physical=pmm_alloc_page_below(limit);
    if (!physical) { log_text("VMX.NO_REGION\n");return; }
    if (!vmx_memory_region_wb(physical)) {
        log_text("VMX.BLOCKED REGION_EFFECTIVE_TYPE_NOT_WB\n");
        if (!pmm_free_page(physical)) panic("VMX REGION FREE FAILED");
        return;
    }
    volatile uint32_t *region=paging_phys_to_virt(physical);
    for (unsigned i=0;i<1024;++i) region[i]=0;
    region[0]=(uint32_t)caps.basic & 0x7fffffff;
    uint64_t enabled=caps.cr4 | (1ULL<<13);
    __asm__ volatile("mov %0,%%cr4"::"r"(enabled):"memory");
    unsigned char cf,zf;
    __asm__ volatile("vmxon %2; setc %0; setz %1"
                     :"=qm"(cf),"=qm"(zf):"m"(physical):"cc","memory");
    if (cf || zf) {
        log_text(cf ? "VMX.VMXON_FAIL_INVALID\n" : "VMX.VMXON_FAIL_VALID\n");
    } else {
        log_text("VMX.ROOT_ENTERED\n");
#ifdef VMX_GUEST_TEST
#ifdef VMX_NESTED_VCPU_TEST
        struct nested_vcpu vcpu={0};
#endif
        uint64_t vmcs=pmm_alloc_page_below(limit);
        if (!vmcs) log_text("VMX.GUEST_BLOCKED NO_VMCS_REGION\n");
        else if (!vmx_memory_region_wb(vmcs))
            log_text("VMX.GUEST_BLOCKED VMCS_EFFECTIVE_TYPE_NOT_WB\n");
#ifdef VMX_NESTED_VCPU_TEST
        else (void)vmx_guest_run_context(&vcpu,vmcs,caps.basic);
#else
        else (void)vmx_guest_run(vmcs,caps.basic);
#endif
        /* guest_run must leave no current VMCS; VMXOFF before freeing any region. */
#endif
        __asm__ volatile("vmxoff; setc %0; setz %1"
                         :"=qm"(cf),"=qm"(zf)::"cc","memory");
        if (cf || zf) panic("VMXOFF FAILED; REGION RETAINED");
#ifdef VMX_EPT_TEST
#ifdef VMX_NESTED_VCPU_TEST
        if(vcpu.busy||(vcpu.vmcs01&&!vcpu.cleared01)||(vcpu.vmcs02&&!vcpu.cleared02))panic("NESTED CTX STILL OWNED RETAIN");
        ept_fixture_destroy(&vcpu.fixture);
#else
        ept_guest_release();
#endif
#endif
#ifdef VMX_GUEST_TEST
        if (vmcs && !pmm_free_page(vmcs)) panic("VMCS REGION FREE FAILED");
        log_text("VMX.ROOT_ROUNDTRIP_PASS (guest result logged separately)\n");
#else
        log_text("VMX.ROOT_ROUNDTRIP_PASS (no guest launched)\n");
#endif
    }
    __asm__ volatile("mov %0,%%cr4"::"r"(caps.cr4):"memory");
    if (!pmm_free_page(physical)) panic("VMX REGION FREE FAILED");
#endif
}
