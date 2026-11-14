#ifndef SYLE_FIRMWARE_VARS_ROM_H
#define SYLE_FIRMWARE_VARS_ROM_H
#include "firmware_apic_transaction.h"
struct fvr_state { uint64_t address; uint8_t original,stage; bool ready,failed; };
/* Fixed pinned4MiB layout, first64KiB VARS block, cold non-SMM/non-SEV profile
 * admitted by caller. Scan original bytes for first non00/50/70, with owned
 * read-only actual EPT checks. No flash device, mutable backing or persistence.
 * Model observes only the two trapped writes, NOT intervening untrapped reads.
 * Fresh zeroed state; no implicit rearm after50 then70. */
bool fvr_init(struct fvr_state*,const struct firmware_memory*);
/* Pure stageable ignored-ROM command: exact byte50 then70 at saved candidate.
 * Backing still must equal original. Rejection leaves model unchanged. */
bool fvr_probe(struct fvr_state*,const struct firmware_memory*,uint64_t gpa,unsigned width,uint64_t value);
/* Only MOV byte store, checked guest WRITE walk + actual read-only VARS EPT.
 * RF completion before RIP; state commits last. Any partial VMWRITE latches
 * failed permanently, no resume/retry. Other rejects unchanged, forbid resume.
 * No source/backing writes, native execution, FVB or secure-storage guarantee. */
struct fvr_trace { unsigned stage,available,width; bool write; uint64_t linear,value; };
enum fat_result fvr_mmio_traced(struct fvr_state*,uint64_t[15],const struct fmm_memory*,
 const struct fat_exit*,bool,void*,bool(*)(void*,uint32_t,uint64_t),struct fvr_trace*);
enum fat_result fvr_mmio(struct fvr_state*,uint64_t native_gpr[15],const struct fmm_memory*,
 const struct fat_exit*,bool exclusive,void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
