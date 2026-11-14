#ifndef SYLE_FIRMWARE_OBSERVER_STACK_H
#define SYLE_FIRMWARE_OBSERVER_STACK_H
#include "firmware_mmio_memory.h"
#define FOS_MAX_FRAMES 4u
/* Read-only bounded heuristic RBP chain, NOT execution approval/unwind proof.
 * Frozen exclusive mode/maps/owned RAM required; no clocks or A/D updates.
 * Diagnostic output must be disjoint trusted memory, never guest-mapped.
 * Only guest virtual addresses/values returned, no host physical/pointer data.
 * Optimized code may not use frame pointers; stop/partial is normal evidence. */
enum fos_stop {FOS_END,FOS_LIMIT,FOS_BAD_ADDRESS,FOS_UNREADABLE,FOS_CHAIN};
struct fos_frame {uint64_t frame,previous,return_rip;bool return_executable;};
struct fos_result {struct fos_frame frames[FOS_MAX_FRAMES];unsigned count;
 enum fos_stop stop;bool top_requested,top_readable,top_executable;uint64_t top_return;};
bool fos_read_word(const struct fmm_memory*,uint64_t va,uint64_t*value);
bool fos_observe(const struct fmm_memory*,uint64_t rbp,uint64_t rsp,bool top,
 struct fos_result*);
#endif
