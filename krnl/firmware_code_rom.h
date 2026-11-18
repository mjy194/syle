#ifndef SYLE_FIRMWARE_CODE_ROM_H
#define SYLE_FIRMWARE_CODE_ROM_H
#include "firmware_apic_transaction.h"
struct fcode_state { uint64_t address; uint8_t original,stage; bool ready,failed; };
/* Cold pinned firmware profile, exclusive frozen ownership/page tables.
 * Bind the first non00/50/70 byte in CODE's first64KiB. Fresh zeroed state only.
 * Two byte stores,50 then70, leave ROM unchanged. Reads run natively and are
 * not receipt-verified by this model. No flash programming or persistence. */
bool fcode_init(struct fcode_state*,const struct firmware_memory*);
/* Decode actual MOV, independently admit CODE RX EPT and guest WRITE mapping.
 * Clear RF before advancing RIP; publish stage last. Any failed VMWRITE latches
 * failed, forbidding resume/retry even if the failing callback changed VMCS.
 * Rejection publishes nothing and must also stop the caller, never resume.
 * No GPR, backing, EPT or guest page-table writes. */
enum fat_result fcode_mmio(struct fcode_state*,const uint64_t native_gpr[15],
 const struct fmm_memory*,const struct fat_exit*,bool exclusive,
 void*,bool(*write)(void*,uint32_t,uint64_t));
#endif
