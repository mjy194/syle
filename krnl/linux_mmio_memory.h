#ifndef SYLE_LINUX_MMIO_MEMORY_H
#define SYLE_LINUX_MMIO_MEMORY_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
struct lmm_memory {
 void *ctx;
 /* Trust boundary: copies only owned private guest RAM. No GPA cast, MMIO,
  * callbacks that fault, or externally mutable page tables during operation. */
 bool (*read)(void*,uint64_t gpa,void*,size_t);
 uint64_t ram_size,cr0,cr3,cr4,efer,cs_base;
 uint32_t cs_ar;uint16_t cs_selector;
};
enum lmm_access { LMM_READ,LMM_WRITE,LMM_EXEC };
/* Physical32, CPL0, flat 64-bit CS, four-level paging only. 4K/2M pages;
 * no 1GiB/PCID/LA57/SMEP/SMAP/PKU/CET. Requires hardware-set A throughout
 * and D on a write leaf; never writes guest page tables. Output on success. */
bool lmm_translate(const struct lmm_memory*,uint64_t linear,enum lmm_access,uint64_t *gpa);
/* Read executable private RAM, up to 15 bytes, without fetching beyond either
 * caller limit or a permission/mapping boundary. Returns available prefix.
 * Decoder must prove it consumed a complete instruction within this count. */
size_t lmm_fetch(const struct lmm_memory*,uint64_t rip,uint8_t bytes[15]);
/* No read from the MMIO target. Data must lie in one 4K page and explicit
 * registered window; validate decoder GLA against exit GLA and every translated
 * byte against exit GPA. Strict reason48/q=0x181(read) or0x182(write), no EPT
 * permissions, no vectoring, no entry failure. Never trusts exit insn length. */
bool lmm_validate(const struct lmm_memory*,uint64_t decoded_linear,unsigned width,bool write,
 uint32_t exit_reason,uint64_t qualification,uint64_t exit_linear,uint64_t exit_gpa,
 uint32_t vectoring,uint64_t window_base,uint64_t window_size);
#endif
