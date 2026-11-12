#ifndef SYLE_DOMAIN_TYPED_LEASE_H
#define SYLE_DOMAIN_TYPED_LEASE_H
#include <stdbool.h>
#include <stdint.h>
#define DTL_ROOTS 16u
#define DTL_ALIASES 32u
#define DTL_REFILLS 32u
struct dtl_root {uint64_t id,generation,hpa,owner,residency,scan_digest;unsigned kind;bool tables_wb;unsigned scanned_aliases;};
struct dtl_alias {uint64_t root_id,root_generation,gpa,hpa,raw;unsigned type,perms;bool ipat;unsigned effective_perms;};
struct dtl_refill {uint64_t id,generation,root_id,owner,root_generation;};
/* Host lease is previously audited externally; oldraw checks here bind target
 * and WB/RW/NX shape, not full host paging/MAXPHYADDR architectural validation. */
struct dtl_host {uint64_t target,table,oldraw;unsigned index;};
struct dtl_inventory {
 uint64_t domain,incarnation,epoch,participants;
 struct dtl_host host;
 unsigned roots,aliases,refills;
 struct dtl_root root[DTL_ROOTS];struct dtl_alias alias[DTL_ALIASES];struct dtl_refill refill[DTL_REFILLS];
 /* Trusted caller audit attestations, not proofs computed by this model.
  * Scans must include every live/spare/historical root and software refill
  * source. target is exclusively data, never code/stack/table/mailbox. */
 bool complete,dedicated_data_wb;
};
enum dtl_phase {DTL_OPEN,DTL_DETACH,DTL_BREAK,DTL_TLB1,DTL_CACHE1,DTL_MAKE,DTL_TLB2,DTL_CACHE2,DTL_PUBLISH,DTL_STAMP,DTL_POISON};
struct dtl_ticket {uint64_t domain,incarnation,epoch;unsigned source,destination;};
struct domain_typed_lease;
struct dtl_token {struct domain_typed_lease *lease;struct dtl_token *next;uint64_t owner,nonce,epoch;unsigned type,root_index;uint64_t root_generation;bool live,prepared;};
struct domain_typed_lease {
 struct dtl_inventory inventory;enum dtl_phase phase;unsigned type,destination,nonwb_type;
 uint64_t retired;
 uint64_t epoch,nonce,pins,detached,revoked,published,cpu_acks;
 uint64_t root_acks[DTL_ROOTS];struct dtl_token *tokens;
};
/* Pure serialized policy; caller supplies real audit and action evidence.
 * Inventory copied at init and immutable thereafter. No hardware operation,
 * cache-type validation, memory scan, lock or physical ownership is provided. */
bool dtl_init(struct domain_typed_lease *,const struct dtl_inventory *);
/* Explicit 0=UC or 4=WT target policy; initial inventory is still WB. */
bool dtl_init_type(struct domain_typed_lease *,const struct dtl_inventory *,unsigned nonwb_type);
/* Stopped-consumer coordinator only: monotonic actual history from a frozen
 * directory. Membership/identity/token indices never change. No hardware proof
 * is performed here; caller holds all consumer entry gates closed. */
bool dtl_root_residency_extend(struct domain_typed_lease *,unsigned root_index,
 uint64_t root_id,uint64_t root_generation,uint64_t new_mask);
bool dtl_acquire(struct domain_typed_lease *,uint64_t owner,unsigned root_index,uint64_t epoch,unsigned type,bool ipat,bool ad,struct dtl_token *,uint64_t *nonce);
bool dtl_admit(struct domain_typed_lease *,struct dtl_token *,uint64_t owner,unsigned root_index,uint64_t nonce,uint64_t epoch,unsigned type,bool ipat,bool ad);
bool dtl_retire_root(struct domain_typed_lease *,unsigned root_index,bool vmcs_gone,bool refills_revoked,bool flushed);
bool dtl_release(struct domain_typed_lease *,struct dtl_token *,uint64_t owner,uint64_t nonce,uint64_t retired_roots,uint64_t revoked_refills);
bool dtl_prepare_token(struct domain_typed_lease *,struct dtl_ticket,struct dtl_token *,uint64_t nonce);
bool dtl_begin(struct domain_typed_lease *,unsigned destination,uint64_t rooted,uint64_t closed,uint64_t drained,struct dtl_ticket *);
bool dtl_detach(struct domain_typed_lease *,struct dtl_ticket,unsigned root_index);
bool dtl_revoke(struct domain_typed_lease *,struct dtl_ticket,unsigned refill_index);
/* DETACH: after detach/revoke, actual local INVEPT; PUBLISH: after publication.
 * Every historically resident CPU must ACK every corresponding root. */
bool dtl_root_ack(struct domain_typed_lease *,struct dtl_ticket,unsigned root_index,unsigned cpu);
bool dtl_publish_root(struct domain_typed_lease *,struct dtl_ticket,unsigned root_index);
bool dtl_cpu_ack(struct domain_typed_lease *,struct dtl_ticket,enum dtl_phase,unsigned cpu);
/* advance BREAK/MAKE attests actual host PTE operation matching saved lease;
 * other phases require complete ACK sets. STAMP attests all new stamps ready. */
bool dtl_advance(struct domain_typed_lease *,struct dtl_ticket,enum dtl_phase);
void dtl_fail(struct domain_typed_lease *);
/* No free action. Caller attests every VMCS retired, all inventory roots
 * invalidated and all participants parked; only restored WB can qualify. */
bool dtl_release_ready(const struct domain_typed_lease *,uint64_t retired,uint64_t root_flush_mask,uint64_t parked);
#endif
