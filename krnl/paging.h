#ifndef SYLE_PAGING_H
#define SYLE_PAGING_H
#include "../include/syle/boot.h"
void paging_init(const struct syle_boot_info *boot);
uint64_t paging_root(void);
/* Only for admitted mapped RAM, after paging_init. Not a mapping operation. */
void *paging_phys_to_virt(uint64_t physical);
/* Diagnostic-only, BSP before AP start: turn an owned low trampoline page RX
 * and map an otherwise absent local APIC page UC. Reject RAM aliases. */
int paging_smp_prepare(uint64_t trampoline,uint64_t lapic);

/* Irreversible marker before issuing any INIT/SIPI, including failed starts. */
void paging_note_ap_start(void);
int paging_ap_start_attempted(void);
/* Read-only proof for one entire aligned page: current four-level host CR3,
 * matching translation, effective supervisor RW, and PAT slot zero WB.
 * No mapping creation. Caller keeps host tables/PAT/MTRRs immutable. */
int paging_page_rw_wb(uint64_t physical);

/* BSP only, interrupts off, current owned root, PGE/PCID off, before any AP start. For an
 * already populated dedicated code page: irreversibly revoke writing from
 * ALL physical aliases and flush the BSP TLB before an RX alias is published.
 * Huge aliases are split; unaligned/unmapped/unsupported contexts fail closed.
 * Caller must not recreate writable aliases or recycle the sealed page. This
 * is not a runtime JIT, writable remapping API, or multicore shootdown. */
int paging_seal_code_page(uint64_t physical);
#endif
