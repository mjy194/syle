#ifndef SYLE_NESTED_DOMAIN_MEMORY_H
#define SYLE_NESTED_DOMAIN_MEMORY_H
#include "ept.h"
#include "nested_ept.h"
/* BSP-built shared GPA map, identical on both physical EPT01s.
 * Fixture pages belong ONLY to this owner. Per-vCPU fixture views borrow them.
 * WB/4K/AD0, 63 pages, no guest EPT12 writers; one explicit stopped-domain data remap. */
struct nested_domain_memory {struct ept_fixture backing;uint64_t root01[2][4],root02[2][4];uint64_t replacement_hpa,retained_hpa;bool ready,contexts_prepared,eviction_prepared,atomic_ad_prepared,atomic_catalog_retained,typed_uc_prepared,typed_contexts_prepared,typed_dynamic_prepared;};
/* Fresh object only. A partial failure retains every still-owned allocation;
 * do not retry on that object or release it through the ready-domain API. */
bool nested_domain_memory_prepare(struct nested_domain_memory *);
/* BSP only, before either CPU starts. Adds immutable EPT12 root B at
 * GPA14000..17000, sharing the domain map, with B:f000 -> b000.
 * Existing prebuilt EPT02s remain context A; adapter builds B composition.
 * Cannot combine this fixture extension with the single-root remap API. */
bool nested_domain_memory_prepare_contexts(struct nested_domain_memory *);
/* BSP only, after successful prepare_contexts, before either CPU starts.
 * Adds C at GPA18000..1b000, C:f000 -> 1c000. One invocation only. */
bool nested_domain_memory_prepare_eviction(struct nested_domain_memory *);
/* BSP prepublication only, once. Validates immutable structure/RO guest aliases
 * and initial clear A/D; does not read EPT12 after publication or synchronize
 * updaters. Runtime uses separate atomic lease/CAS authority, never this helper. */
/* Prepublication only: strict base fixture, dedicated GPA f000 data. Sets
 * IPAT on every present WB leaf in both 01/02, retained fixture and virtual12. WB/AD0
 * otherwise unchanged. Runtime must audit all execution/spare/history roots. */
bool nested_domain_memory_prepare_typed_uc(struct nested_domain_memory *);
/* Explicit prepublication two-root UC profile: shared f000 backing,
 * both roots immutable, WB witness 30000 differs by root. No eviction. */
bool nested_domain_memory_prepare_typed_contexts(struct nested_domain_memory *);
/* Explicit prepublication three-root typed/eviction fixture, from fresh base.
 * Reuses typed_contexts validation, then performs only infallible stores.
 * Guest aliases for all EPT12 tables are RO; L0 may transact the f000 leaf
 * type under both closed gates. A/B/C share f000 and witness 30000 selects
 * backing GPA30000/31000/32000. All initial leaves are WB/IPAT1/AD0. */
bool nested_domain_memory_prepare_typed_dynamic(struct nested_domain_memory *);
/* Final BSP setup check, before either consumer enters. No writes. */
bool nested_domain_memory_typed_ready(const struct nested_domain_memory *);
bool nested_domain_memory_prepare_atomic_ad(struct nested_domain_memory *);
/* Borrowed execution view: never pass it to ept_fixture_destroy/owned_release.
 * Only this domain owner releases the backing, after BOTH CPUs are inactive. */
bool nested_domain_memory_view(struct nested_domain_memory *,unsigned cpu,struct ept_fixture *);
/* BSP only, before vCPU start. Zeroed dedicated WB page; caller may fill it.
 * Failure leaves the domain unchanged. Only one replacement is supported. */
bool nested_domain_memory_prepare_replacement(struct nested_domain_memory *);
/* Caller holds exclusive domain ownership, has closed BOTH entry gates and
 * stopped both users of EPT01/EPT02. gate_closed is a caller assertion, NOT
 * synchronization performed here. On success caller must invalidate all four
 * EPT contexts on their owning CPUs before opening either gate. No INVEPT here.
 * Only GPA f000; prevalidation failure changes nothing. Borrowed view pages[]
 * are snapshots and must be refreshed or bypassed by authoritative resolution. */
bool nested_domain_memory_remap_data(struct nested_domain_memory *,uint64_t gpa,bool gate_closed);
void nested_domain_memory_release(struct nested_domain_memory *); /* both owners clear/off/park */
#endif
