#ifndef SYLE_LINUX_CPU_PROFILE_H
#define SYLE_LINUX_CPU_PROFILE_H
#include "linux_vcpu_policy.h"
enum lcp_kind {LCP_BASELINE=0,LCP_EXPERIMENTAL_APIC=1};
/* Pure profile definition, NOT authorization to activate it. Experimental
 * selection requires separately validated runtime+firmware/domain evidence.
 * The default lvp API and its CPUID bytes remain unchanged. Every capability
 * query and CPUID exit for a selected profile must call this same function. */
bool lcp_query(enum lcp_kind,const struct lvp_cpu_features*,uint32_t leaf,uint32_t subleaf,struct lvp_cpuid_result*);
/* Real CPL0 CPUID exit, no pending event/vectoring; caller owns fixed profile.
 * Software GPR commits only after RIP write. False forbids resume, including
 * an uncertain VMWRITE. Native order AX BX CX DX SI DI BP R8..R15. */
bool lcp_apply(enum lcp_kind,const struct lvp_cpu_features*,uint64_t gpr[15],uint64_t rip,unsigned cpl,uint32_t reason,unsigned length,void*,bool(*)(void*,uint32_t,uint64_t));
#endif
