#ifndef SYLE_FIRMWARE_CLOCK_APIC_TRANSACTION_H
#define SYLE_FIRMWARE_CLOCK_APIC_TRANSACTION_H
#include "firmware_apic_transaction.h"
#include "firmware_timer_100m_mmio.h"
/* Separate nondefault profile; reference is a fresh admitted Hv sample.
 * Owner syncs at entry/exit/MMIO, clock lifecycle is never rolled back. */
struct fct_state { struct fag_state device; bool failed; struct ft100_mmio clock; uint64_t epoch,reference; };
enum fat_result fct_mmio(struct fct_state*,uint64_t[15],const struct fmm_memory*,const struct fat_exit*,uint32_t,uint32_t,bool,void*,bool(*)(void*,uint32_t,uint64_t));
enum fat_result fct_mmio_traced(struct fct_state*,uint64_t[15],const struct fmm_memory*,const struct fat_exit*,uint32_t,uint32_t,bool,void*,bool(*)(void*,uint32_t,uint64_t),struct fat_trace*);
#endif
