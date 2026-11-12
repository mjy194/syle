#ifndef SYLE_NESTED_ATOMIC_MEMORY_H
#define SYLE_NESTED_ATOMIC_MEMORY_H
#include "nested_domain_memory.h"
#include "nested_root_catalog.h"
#define NAM_OWNED_PAGES (EPT_GUEST_PAGES+20)
/* Stable, fresh, exclusively owned storage. No backing-release API. The
 * domain, this object and all backing stay retained for the diagnostic's life.
 * Pass this object as the source authority context. Never publish an AD0
 * reader/catalog over these source pages; all later word accesses are atomic. */
typedef bool (*nam_uc_guard_fn)(void *,uint64_t target_hpa,uint64_t type_epoch);
struct nested_atomic_memory {
 struct nested_root_catalog catalog;
 struct nrc_span spans[EPT_GUEST_PAGES];
 struct nrc_root roots[3];
 struct nrc_identity identity;
 struct nested_domain_memory *domain;
 uint64_t owned[NAM_OWNED_PAGES];
 uint64_t sparse_tables[10]; /* N45 retained physical map PTs; never released. */
 bool prepared,large_2m,large_1g;
 bool fixed_uc,uc_staged,uc_detached,uc_installed;
 uint64_t type_epoch;
 nam_uc_guard_fn uc_guard;
 void *uc_context;
};
bool nested_atomic_memory_prepare(struct nested_atomic_memory *,
 struct nested_domain_memory *,struct nrc_identity);
bool nested_atomic_memory_prepare_2m(struct nested_atomic_memory *,
 struct nested_domain_memory *,struct nrc_identity);
bool nested_atomic_memory_prepare_1g(struct nested_atomic_memory *,
 struct nested_domain_memory *,struct nrc_identity);
/* N46B staged bootstrap only. stage validates the original all-WB fixture,
 * builds unpublished fixed-UC metadata and irrevocably retains all backing.
 * No source/catalog consumer may start before publish. Only the dedicated
 * L1 GPA f000 may become UC. Existing prepare APIs remain WB-only.
 * Caller gates all guests, proves physical owner inventory and drives actual
 * both-owner INVEPT/TLB/cache maintenance between these steps. These functions
 * perform no hardware operation and cannot manufacture those receipts. */
bool nested_atomic_memory_uc_stage(struct nested_atomic_memory *,
 struct nested_domain_memory *,struct nrc_identity,uint64_t type_epoch);
bool nested_atomic_memory_uc_staged(const struct nested_atomic_memory *);
bool nested_atomic_memory_uc_detach(struct nested_atomic_memory *);
/* After completed host UC make and both CPUs' TLB/cache/current-UC proofs.
 * guard audits current exact host UC policy/lifetime without changing it.
 * Install makes final source/fixed EPT words but does NOT publish catalog. */
bool nested_atomic_memory_uc_install(struct nested_atomic_memory *,nam_uc_guard_fn,void *);
bool nested_atomic_memory_uc_installed(const struct nested_atomic_memory *);
/* Only AFTER both owners invalidate all fixed EPTPs again and final audit. */
bool nested_atomic_memory_uc_publish(struct nested_atomic_memory *);
/* Read-only live inventory, mapping and structural audit. Allows source A/D. */
bool nested_atomic_memory_ready(const struct nested_atomic_memory *);
#endif
