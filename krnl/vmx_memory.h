#ifndef SYLE_VMX_MEMORY_H
#define SYLE_VMX_MEMORY_H
#include <stdint.h>
#define VMX_MTRR_MAX_VARIABLE 256u
struct vmx_mtrr_pair { uint64_t base, mask; };
/* Raw read-only snapshot; fixed[] follows MSRs 250,258,259,268..26f. */
struct vmx_memory_snapshot {
    unsigned physical_bits;
    unsigned cpu_mtrr, cpu_pat;
    uint64_t cap, def_type, pat, cr0;
    uint64_t fixed[11];
    struct vmx_mtrr_pair variable[VMX_MTRR_MAX_VARIABLE];
};
/* Proves a whole aligned 4 KiB region is WB with PAT slot 0.
 * Caller must map the region with PAT index 0 (PCD=PWT=PAT=0), must use
 * ordinary allocator-owned RAM, and must not change MTRRs/PAT concurrently.
 * Unsupported or malformed states fail closed. No writes to CPU state. */
int vmx_memory_snapshot_region_wb(const struct vmx_memory_snapshot *s,
                                  uint64_t physical);
/* Kernel-only snapshot acquisition and policy check. */
int vmx_memory_region_wb(uint64_t physical);
#endif
