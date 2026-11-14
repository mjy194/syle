#ifndef SYLE_FIRMWARE_MEMORY_H
#define SYLE_FIRMWARE_MEMORY_H
#include "firmware_layout.h"
#include "simple_guest.h"
#define FWM_RAM_SIZE (128u*1024u*1024u)
#define FWM_RAM_PAGES (FWM_RAM_SIZE/4096u)
#define FWM_FLASH_MAX (4u*1024u*1024u)
#define FWM_FLASH_PAGES (FWM_FLASH_MAX/4096u)
#define FWM_TABLE_PAGES 70u
#define FWM_HASH_SLOTS 65536u
/* Fresh zeroed, static/heap object only (~800KiB). No VMX is executed.
 * Allocator/map/writeback callbacks remain trusted: unique exclusively-owned
 * pages, stable private mappings and WB proof, with callbacks alive until all
 * releases succeed. Image buffers and callbacks must remain immutable and
 * disjoint from allocated backing throughout prepare. Duplicate/invalid tokens
 * are caught before map/mutation and quarantine the context; automatic
 * release then refuses to guess
 * ownership. rejected_token is diagnostic, not an additional owned allocation.
 */
struct firmware_memory {
 struct sg_memory_ops owner;
 uint64_t ram[FWM_RAM_PAGES],flash[FWM_FLASH_PAGES],tables[FWM_TABLE_PAGES];
 uint64_t owned_hash[FWM_HASH_SLOTS];
 struct fwl_plan layout;
 uint64_t eptp,rejected_token;
 unsigned flash_pages,physical_bits;
 bool prepared,quarantined;
};
bool fwm_prepare(struct firmware_memory*,const struct sg_memory_ops*,
 const void*code,size_t code_size,const void*vars,size_t vars_size,unsigned physical_bits);
/* All backing owned by this instance, including EPT tables. Exact aligned
 * physical-page query; does not accept an interior byte pointer. */
bool fwm_owned_page(const struct firmware_memory*,uint64_t);
/* Access only low RAM, never CODE/VARS/MMIO. Whole range checked before copying.
 * Callback map failure can make writes partial; caller must fail-stop. */
bool fwm_read_ram(struct firmware_memory*,uint64_t,void*,size_t);
bool fwm_write_ram(struct firmware_memory*,uint64_t,const void*,size_t);
/* If ever installed, caller MUST prove VMCLEAR, INVEPT and VMXOFF complete and
 * no vCPU/device references remain. Partial release retains exact tokens for
 * retry. No automatic reclamation of quarantined allocator violations. */
bool fwm_release(struct firmware_memory*);
#endif
