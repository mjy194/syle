#ifndef SYLE_MEMORY_H
#define SYLE_MEMORY_H
#include <stdint.h>
#include "../include/syle/boot.h"
#define PMM_PAGE_SIZE UINT64_C(4096)
#define PMM_DIRECT_MAP_LIMIT (UINT64_C(1) << 47)
#define PMM_MAX_RANGES 4096u
#define PMM_MAX_DESCRIPTORS 1024u
struct pmm_stats {
    uint64_t usable_pages; /* after reservations; free + allocated */
    uint64_t free_pages;
    uint64_t allocated_pages;
};
/* Bootstrap CPU only, interrupts disabled. No synchronization or firmware
 * reclamation. Init is fail-closed; reinitialization is refused while live.
 * Caller supplies readable, trusted boot_info and map backing memory. */
int pmm_init(const struct syle_boot_info *boot);
/* Returns an aligned physical page, or 0. Does not touch/zero page contents. */
uint64_t pmm_alloc_page(void);
/* Exclusive CPU/4-level direct-map physical bound. */
uint64_t pmm_address_limit(void);
/* Physical address constraints, upper exclusive / lower inclusive. */
uint64_t pmm_alloc_page_below(uint64_t limit);
uint64_t pmm_alloc_page_above(uint64_t min);
/* Only a currently allocated page can be freed. Returns 1 on success, 0 on
 * misuse or exhausted extent metadata; failure leaves ownership unchanged. */
int pmm_free_page(uint64_t address);
struct pmm_stats pmm_get_stats(void);
#endif
