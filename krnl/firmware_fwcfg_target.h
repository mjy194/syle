#ifndef SYLE_FIRMWARE_FWCFG_TARGET_H
#define SYLE_FIRMWARE_FWCFG_TARGET_H
#include "firmware_mmio_memory.h"
/* Snapshot lifetime: single stopped vCPU, immutable owned mappings and page
 * tables until commit completes. Caller must not expose pointer to guest.
 * No writes occur here. Commit is irreversible: after byte copies start, any
 * architectural commit failure must fail-stop, never retry or roll back. */
struct fft_plan { uint8_t *bytes; uint64_t gpa; unsigned count; };
/* Forward single-page span of 1..64 bytes only. Every byte must translate with
 * FMM_WRITE, to continuous owned low RAM. Reject any destination backing page
 * used by this target walk or either of the two instruction-byte walks
 * (root/intermediates, including HPA aliases), and the instruction pages.
 * This is not a global check of every guest page table.
 * False leaves plan unchanged. Caller handles REP/DF/instruction admission. */
bool fft_prepare(const struct fmm_memory*,uint64_t linear,unsigned count,uint64_t rip,struct fft_plan*);
#endif
