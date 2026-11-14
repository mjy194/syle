#ifndef SYLE_FIRMWARE_CPU_MODE_DE_H
#define SYLE_FIRMWARE_CPU_MODE_DE_H
#include "firmware_cpu_mode.h"
/* Independent opt-in DE configuration only; NOT debug virtualization.
 * Caller supplies real host CPUID leaf1 and actual VMCS guest DEBUGCTL/DR7 and
 * primary controls under exclusive ownership. All MOV-DR exits must STOP,
 * including DR4/5: interception precedes their DE #UD/CPL #GP checks.
 * Never clear primary23; native does not preserve arbitrary guest DR0-3.
 * CPUID guest DE exposure requires separate coordinated admission. No host
 * debug access, VMCS writes, opcode validation or CPUID changes occur here.
 * Every later CR0/3/4/EFER transition in this profile must use this wrapper so
 * guest readback shadow and hardware CR4 carry the same DE bit. Default fcm
 * remains unchanged. Caller must separately validate actual owned MOV-CR bytes.
 */
struct fcmd_gate {uint32_t host_leaf1_edx;uint64_t primary_controls,debugctl,dr7,pending_debug,entry_controls,exit_controls,cr4_fixed0,cr4_fixed1;};
bool fcmd_write(const struct fcm_state*,uint64_t ram_size,enum fcm_register,
 uint64_t value,const struct fcmd_gate*,struct fcm_state*);
#endif
