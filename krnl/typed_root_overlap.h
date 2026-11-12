#ifndef SYLE_TYPED_ROOT_OVERLAP_H
#define SYLE_TYPED_ROOT_OVERLAP_H
#include "typed_root_local_provider.h"
/* N39B2 diagnostic protocol. Each record has one physical-owner writer and
 * becomes immutable before its release publication. Readers acquire first.
 * These records attest actual runtime events; they never authorize freeing.
 * No pool/item pointer may be followed through a peer record. */
struct tr_overlap_counts {uint64_t creations,evictions,misses,switches,switch_sequence;};
struct tr_overlap_switch {
 uint64_t sequence,pin,write,unpin,old_eptp,new_eptp,completed;
 unsigned stage;
};
struct tr_overlap_hold {
 struct tr_local_completed completed;
 uint64_t version;
 unsigned phase,cpu;
};
struct tr_overlap_receipt {
 struct tr_local_completed completed;
 struct td_receipt receipt; /* Value copy; links are never dereferenced. */
 uint64_t evidence_version;
};
struct tr_overlap_peer {
 struct tr_overlap_receipt retired;
 struct tr_overlap_counts before,after;
 struct tr_overlap_switch switched;
 unsigned phase,cpu;
};
struct tr_overlap {
 struct tr_overlap_hold hold;
 struct tr_overlap_peer peer;
 struct tr_overlap_receipt final;
 uint32_t held,progress,finished;
};
struct tr_overlap_wait {
 uint64_t (*now)(void *);
 void (*relax)(void *);
 bool (*healthy)(void *),(*lock_owned)(void *);
 void *context;
 uint64_t budget;
};
/* Call each step only AFTER the corresponding real successful operation.
 * Failure retains both VMCS references. A distinct old/new root is required
 * to count as a switch; ordinary same-root VMRESUMEs cannot prove overlap. */
bool tr_overlap_switch_step(struct tr_overlap_switch *,unsigned,uint64_t,uint64_t);
bool tr_overlap_publish_hold(struct tr_overlap *,struct tr_overlap_hold);
bool tr_overlap_publish_peer(struct tr_overlap *,struct tr_overlap_peer);
bool tr_overlap_finish(struct tr_overlap *,struct tr_overlap_receipt);
bool tr_overlap_valid(const struct tr_overlap *);
bool tr_overlap_receipt_equal(struct tr_overlap_receipt,struct tr_overlap_receipt);
bool tr_overlap_wait_for(const uint32_t *,const struct tr_overlap_wait *);
#endif
