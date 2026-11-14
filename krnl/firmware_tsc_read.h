#ifndef SYLE_FIRMWARE_TSC_READ_H
#define SYLE_FIRMWARE_TSC_READ_H
#include "firmware_irq_apic.h"
/* Explicit virtual 10MHz RDTSC = admitted reference - boot origin, not host
 * TSC or a calibration/invariant-TSC claim. Fresh once at boot, serialized,
 * stable domain/epoch/CPU source admission supplied by native clock_sync.
 * No reference sampling here. No RDTSCP/MSR10/offset-write support; those exits
 * remain fail-stop, so this is not a complete architectural TSC service.
 * Strict long CPL0 owned0f31/len2; actual CR4 mode comes from fmm. CR4.TSD is
 * not bypassed for user mode (CPL0 only); unadmitted mode bits still reject.
 * Each existing clock_sync feeds ftsc_sample; read must use its exact last
 * reference. Long gaps between RDTSC instructions are allowed when samples
 * remain continuous. No extra source reads. last is never rolled back.
 * RF->RIP then publish
 * zero-extended EAX/EDX. Partial write/reentry poisons state; native quarantines
 * entire session. No IRQ/clock snapshot publication or host counter access.
 */
struct ftsc_state {uint64_t domain,epoch,origin,last,max_gap;uint32_t cpu;bool ready,busy,failed;};
struct ftsc_sample {uint64_t domain,epoch,reference;uint32_t cpu;bool source_admitted;};
bool ftsc_init(struct ftsc_state*,uint64_t domain,uint64_t epoch,uint64_t boot_reference,uint64_t max_gap,uint32_t cpu);
bool ftsc_sample(struct ftsc_state*,uint64_t domain,uint64_t epoch,uint32_t cpu,uint64_t reference);
enum fat_result ftsc_read(struct ftsc_state*,const struct ftsc_sample*,uint64_t[15],
 const struct fmm_memory*,const struct fat_exit*,unsigned length,bool exclusive,
 void*,lei_write);
#endif
