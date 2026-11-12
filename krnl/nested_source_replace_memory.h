#ifndef SYLE_NESTED_SOURCE_REPLACE_MEMORY_H
#define SYLE_NESTED_SOURCE_REPLACE_MEMORY_H
#include "nested_root_catalog.h"
#define NSRM_MAX_PAGES 16u
#define NSRM_MAX_ROOTS 16u
struct nsrm_request {
 /* l2_gpa is the exact normalized 4K page, not an interior address. */
 uint64_t ticket,full_eptp,l2_gpa,table_gpa,table_hpa,table_object,table_lifetime;
 uint64_t expected_old,new_leaf,new_gpa,new_hpa,new_object,new_lifetime;
 uint16_t index;
};
/* Independent stopped-domain management authority, NOT the drained reader
 * resolver. Success attests old updater drained, owned
 * pinned WB/IPAT1 backing retained forever, and exclusive structural access
 * through apply. Ticket is an immutable coordinator transaction identity.
 * TABLE returns a whole atomic writable L0 page and attests ALL guest
 * aliases RO. DATA returns no view and may have writable guest aliases.
 * replacement_data_safe attests the destination DATA page is authorized for
 * replacement and is neither code nor stack nor table backing. Other graph
 * DATA leaves need not carry this destination-only attestation. Unchanged
 * graph leaves may alias TABLE backing only with exactly R permission and
 * full TABLE authority; neither old nor new replacement target may be TABLE. */
struct nsrm_authority_page {
 struct nrc_identity identity;
 uint64_t ticket,gpa,object,lifetime,hpa;
 uint64_t *atomic_view;
 uint8_t permissions,memory_type;
 bool ignore_pat,pinned,guest_aliases_ro,updater_drained,replacement_data_safe;
};
typedef bool (*nsrm_authority_fn)(void *,uint64_t,const struct nrc_span *,uint64_t,struct nsrm_authority_page *);
struct nsrm_snapshot {uint64_t gpa,hpa,*view,words[512];unsigned depth;bool reached;};
/* Zero-initialized, stable exclusive storage. Do not copy or edit a prepared
 * plan, request, catalog or authority context. No backing release API exists.
 * started means the sole CAS was attempted; any false after started requires
 * coordinator poison, never rollback. All backing survives every failure. */
struct nsrm_plan {
 struct nsrm_plan *self;
 struct nested_root_catalog *catalog;
 struct nrc_identity identity;
 nsrm_authority_fn authority;
 void *context;
 struct nsrm_request request;
 struct nsrm_snapshot pages[NSRM_MAX_PAGES];
 size_t page_count;
 unsigned target;
 bool ready,started,applied;
};
bool nsrm_prepare(struct nsrm_plan *,struct nested_root_catalog *,nsrm_authority_fn,void *,struct nsrm_request);
bool nsrm_apply(struct nsrm_plan *);
#endif
