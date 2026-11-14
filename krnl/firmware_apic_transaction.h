#ifndef SYLE_FIRMWARE_APIC_TRANSACTION_H
#define SYLE_FIRMWARE_APIC_TRANSACTION_H
#include "firmware_apic_guard.h"
#include "firmware_mmio_memory.h"
struct fat_state { struct fag_state device; bool failed; };
struct fat_exit {
 uint64_t rip,rsp,qualification,linear,physical,ds_base,es_base,ss_base;
 uint64_t rflags,blocking,pending_debug,dr7;
 uint32_t reason,vectoring,entry_event;
};
struct fat_trace { unsigned stage,available,width,reg; bool write; uint64_t store_value; uint32_t lvt,initial; bool armed; };
/* stage: 0 admission,1 fetch,2 decode,3 target,4 device,5 commit,6 committed. */
enum fat_result { FAT_REJECTED=-1,FAT_FATAL=-2,FAT_COMMITTED=0 };
/* Single serialized vCPU, no concurrent guest execution or memory mutation,
 * owned RAM/ROM snapshot, no pending event/reservation. Caller freezes CPU
 * feature exposure APIC=1/x2=0 with BASE/MMIO route; all failures forbid resume.
 * Write callback false may be partial: fatal latch must never be cleared. */
enum fat_result fat_mmio(struct fat_state*,uint64_t native_gpr[15],
 const struct fmm_memory*,const struct fat_exit*,uint32_t cpuid_ecx,uint32_t cpuid_edx,
 bool exclusive,void*,bool(*write)(void*,uint32_t,uint64_t));
enum fat_result fat_mmio_traced(struct fat_state*,uint64_t[15],const struct fmm_memory*,const struct fat_exit*,uint32_t,uint32_t,bool,void*,bool(*)(void*,uint32_t,uint64_t),struct fat_trace*);
#endif
