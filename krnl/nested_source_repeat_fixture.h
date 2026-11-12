#ifndef SYLE_NESTED_SOURCE_REPEAT_FIXTURE_H
#define SYLE_NESTED_SOURCE_REPEAT_FIXTURE_H
#include "nested_atomic_memory.h"
#include "nested_source_replace_memory.h"
#include "typed_root_source_replace.h"
#define NSRF_REPEAT_NONCES UINT64_C(4096)
#define NSRF_REPEAT_P UINT64_C(0xf000)
#define NSRF_REPEAT_Q UINT64_C(0x1d000)
#define NSRF_REPEAT_Q_SENTINEL UINT64_C(0x5566556655665566)
struct nested_source_repeat_fixture;
struct nsrf_repeat_context {struct nested_source_repeat_fixture *fixture;unsigned bank;};
typedef bool (*nsrf_repeat_guard_fn)(void *,unsigned,struct tsr_ticket);
struct nsrf_repeat_step {
 struct nsrf_repeat_step *self;struct nested_source_repeat_fixture *fixture;
 unsigned index;struct tsr_ticket ticket;
 nsrf_repeat_guard_fn guard;void *context;
 struct nsrm_plan plan;uint64_t data_before[3][512];
 uint64_t catalog_nonce,updater_nonce;
 bool attempted,changed,published;
};
struct nsrf_repeat_bank {
 struct nested_root_catalog catalog;struct nrc_span spans[EPT_GUEST_PAGES];struct nrc_root roots[3];
};
/* Fresh stable exclusive fixture. Two distinct one-shot steps, three source
 * banks/gates, unchanged retained backing. Never reset/reinitialize any bank.
 * Source publication is distinct from the outer round's final entry release. */
struct nested_source_repeat_fixture {
 struct nested_atomic_memory original;
 struct nsrf_repeat_bank banks[2];struct ad_updater_gate gates[3];struct ad_entry_stamp stamps[3];
 struct nrc_identity identities[3];struct nsrf_repeat_context resolver[3];
 struct nsrf_repeat_step steps[2];struct nested_source_repeat_fixture *self;
 nrc_atomic_authority_fn original_authority;
 unsigned structural_generation,published_generation;bool prepared;
};
bool nested_source_repeat_prepare(struct nested_source_repeat_fixture *,struct nested_domain_memory *,const struct nrc_identity[3]);
/* Exact requested source generation, never infer it from returned P bytes. */
bool nested_source_repeat_ready(const struct nested_source_repeat_fixture *,unsigned);
struct nested_root_catalog *nested_source_repeat_catalog(struct nested_source_repeat_fixture *,unsigned);
void *nested_source_repeat_context(struct nested_source_repeat_fixture *,unsigned);
/* Exactly one call per immutable step. Full ticket/current bank/4096 budget
 * snapshots checked before CAS. False after attempted is permanent retain-stop.
 * The external guard proves both physical owners fully stopped and retired. */
bool nested_source_repeat_change(struct nested_source_repeat_fixture *,unsigned,struct tsr_ticket,nsrf_repeat_guard_fn,void *);
/* Exact step's raw source and full P/Q/C stopped-cut DATA, before guest resume. */
bool nested_source_repeat_raw_unchanged(const struct nested_source_repeat_fixture *,unsigned);
#endif
