#ifndef SYLE_NESTED_SOURCE_REPLACE_CHAIN_H
#define SYLE_NESTED_SOURCE_REPLACE_CHAIN_H
#include "nested_source_replace_round.h"
/* N48A: exactly two stable rounds and three distinct metadata banks. All
 * backing is retained. This controller does not free/retype source memory. */
struct nsrc_bank {
 struct nested_root_catalog *catalog;struct ad_updater_gate *gate;
 struct nrc_span *spans;size_t span_capacity;
 struct nrc_root *roots;size_t root_capacity;
 struct nrc_identity identity;struct ad_entry_stamp stamp;
};
struct nested_source_replace_chain;
struct nsrc_change_context {struct nested_source_replace_chain *chain;unsigned index;};
struct nested_source_replace_chain {
 struct nested_source_replace_chain *self;
 struct nested_source_replace_round *rounds[2];struct nsrc_bank banks[3];
 struct nosr_config owner_config[2];
 tsr_change_fn change[2];void *change_context[2];struct nsrc_change_context changes[2];
 void (*lock)(void *),(*unlock)(void *);unsigned (*current_cpu)(void *);void *context;
 struct tsr_successor_handoff handoff;
 uint64_t active_generation,fault,transition[2],completed[2];
};
/* Install once after round0 bootstrap and both initial owner adapters, before
 * either nsrr_run. Round1 is fresh configured storage, never bootstrapped. */
bool nsrc_init(struct nested_source_replace_chain *,struct nested_source_replace_round *,
 struct nested_source_replace_round *,const struct nsrc_bank[3]);
/* Actual call/return sequencing owns departure. No public returned=true API.
 * generation1 returns only after its own departure is release-published.
 * generation2 starts from a NEW complete physical/reflected checkpoint. */
bool nsrc_run(struct nested_source_replace_chain *,unsigned,uint64_t generation);
/* BEFORE all ordinary config/registry reads, with captured generation. */
bool nsrc_before_entry(struct nested_source_replace_chain *,unsigned,uint64_t generation);
#endif
