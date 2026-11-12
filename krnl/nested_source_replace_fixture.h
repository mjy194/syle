#ifndef SYLE_NESTED_SOURCE_REPLACE_FIXTURE_H
#define SYLE_NESTED_SOURCE_REPLACE_FIXTURE_H
#include "nested_atomic_memory.h"
#include "nested_source_replace_memory.h"
#define NSRF_TARGET_GPA UINT64_C(0xf000)
#define NSRF_NEW_GPA UINT64_C(0x1d000)
#define NSRF_CONTROL_GPA UINT64_C(0x1c000)
#define NSRF_NEW_SENTINEL UINT64_C(0x5566556655665566)
/* This is an independent management proof, not the drained reader resolver.
 * Every call must check the exact saved coordinator ticket, both stopped and
 * parked owners, drained old updater gate and exclusive structural access.
 * It must not reenter the coordinator or manufacture hardware receipts. */
typedef bool (*nsrf_management_guard_fn)(void *,uint64_t);
/* Stable fresh exclusive storage; NEVER copy, reset, or release it. All 83
 * original allocations and both metadata banks survive every outcome. The
 * original authority/ready contract is unchanged and rejects the new graph.
 * Use &original as bank-0 source authority context, and this object as bank-1
 * source authority context. The successor is not initialized before drain. */
struct nested_source_replace_fixture {
 struct nested_atomic_memory original;
 struct nested_root_catalog successor;
 struct nrc_span spans[EPT_GUEST_PAGES];
 struct nrc_root roots[3];
 struct nsrm_plan plan;
 struct nested_source_replace_fixture *self;
 struct nrc_identity identity[2];
 nrc_atomic_authority_fn original_authority;
 nsrf_management_guard_fn management_guard;
 void *management_context;
 uint64_t ticket;
 uint64_t data_before[3][512]; /* old, new, C: raw data at the stopped cut */
 bool prepared,attempted,changed,published;
};
/* BSP, after base domain preparation and before either consumer starts.
 * Uses only preowned GPA1d000 ordinary DATA, not code, stacks, paging, C,
 * VMCS or pool storage. Initializes that whole page, sentinel at word zero. */
bool nested_source_replace_fixture_prepare(struct nested_source_replace_fixture *,
 struct nested_domain_memory *,struct nrc_identity old_id,struct nrc_identity new_id);
/* Live structural/ownership audit, allowing only legal source A/D changes.
 * Independently audits each retained page's current host RW/WB mapping path
 * and MTRR/PAT policy, all five fixed physical maps, every
 * source word, exact catalog inventory and the current structural generation. */
bool nested_source_replace_fixture_ready(const struct nested_source_replace_fixture *);
/* Exactly one attempt, from tsr_change_fn after both owners retired. Performs
 * one exact nsrm CAS, raw source/data preservation check, then successor nonce
 * seeding, identical retained inventory registration and publication. A false
 * result after plan.started requires coordinator poison; never rollback.
 * No INVEPT, updater successor initialization or physical publication here. */
bool nested_source_replace_fixture_change(struct nested_source_replace_fixture *,
 uint64_t ticket,nsrf_management_guard_fn,void *,uint64_t reserve_nonces);
/* Read-only exact stopped-cut proof, before resumed guest A/D or DATA writes.
 * Every source word except the target is compared INCLUDING A/D bits, and all
 * old/new/C data words are unchanged from their stopped-cut snapshots. */
bool nested_source_replace_fixture_raw_unchanged(const struct nested_source_replace_fixture *);
/* Independent management callback used by the embedded production nsrm plan.
 * Not an entry point for an ordinary source reader or updater. */
bool nested_source_replace_fixture_authority(void *,uint64_t,
 const struct nrc_span *,uint64_t,struct nsrm_authority_page *);
#endif
