#ifndef SYLE_LINUX_MICROCODE_MODEL_H
#define SYLE_LINUX_MICROCODE_MODEL_H
#include "linux_vcpu_policy.h"
/* Virtual CPU has no update facility and revision zero. This is not the
 * physical CPU's revision/security status. No host MSR access is permitted.
 * Intel SDM Vol.4 IA32_BIOS_SIGN_ID: clear, CPUID.01H, read signature[63:32].
 * We accept only zero writes; all nonzero writes and update trigger 0x79 fail.
 * loaded records completion of the virtual CPUID.01H refresh, not an update. */
struct lmc_state { bool loaded; };
bool lmc_msr(struct lmc_state*,uint64_t[15],uint64_t,unsigned,uint64_t,unsigned,void*,bool(*)(void*,uint32_t,uint64_t));
bool lmc_cpuid(struct lmc_state*,uint64_t[15],uint64_t,unsigned,uint64_t,unsigned,const struct lvp_cpu_features*,void*,bool(*)(void*,uint32_t,uint64_t));
/* False means stop, including partially successful VMWRITE. Software state and
 * GPRs commit only after RIP succeeds. Reset per CREATE with {0}. */
#endif
