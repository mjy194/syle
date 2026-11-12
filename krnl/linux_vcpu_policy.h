#ifndef SYLE_LINUX_VCPU_POLICY_H
#define SYLE_LINUX_VCPU_POLICY_H
#include <stdbool.h>
#include <stdint.h>
/* Conservative profile: no VMX/XSAVE/AVX/FSGSBASE/PCID/LA57/PKU/CET/APIC.
 * Runtime enforces CR/MSR policy on every resume, not merely CPUID hiding. */
struct lvp_cpu_features { uint32_t basic_edx, extended_edx; unsigned physical_bits; };
struct lvp_cpuid_result { uint32_t a,b,c,d; };
bool lvp_cpu_supported(const struct lvp_cpu_features *);
bool lvp_cpuid(const struct lvp_cpu_features *,uint32_t,uint32_t,struct lvp_cpuid_result *);
bool lvp_guest_cr0(uint64_t);
bool lvp_guest_cr4(uint64_t);
#endif
