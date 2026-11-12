#ifndef SYLE_TYPED_DIRECTORY_H
#define SYLE_TYPED_DIRECTORY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Serialized bookkeeping only. All intrusive storage is exclusively owned,
 * stable and live until explicitly detached. No hardware action or lock. */
struct typed_directory;
struct td_identity {
 uint64_t domain,incarnation,root_id,root_incarnation,allocator_lifetime,owner;
};
enum td_state { TD_CANDIDATE, TD_LIVE, TD_RETAINED, TD_RETIRING, TD_RETIRED };
struct td_record {
 struct td_record *next; struct typed_directory *directory;
 struct td_identity identity;
 uint64_t full_eptp,hpa,cache_generation,history,pins,refills,constructing_nonce,retirement_nonce,retirement_epoch;
 enum td_state state; bool attached;
};
struct td_operation {
 struct td_operation *next; struct typed_directory *directory;
 uint64_t nonce; bool live;
};
struct td_receipt {
 struct td_receipt *next; struct typed_directory *directory;
 struct td_identity identity;
 uint64_t full_eptp,hpa,cache_generation,history,acked,flush_serial;
 /* Exact durable commit provenance, independent of later peer mutations. */
 uint64_t type_epoch,retirement_nonce,commit_version;
 bool live;
};
struct td_retire_ticket {struct td_identity identity;uint64_t full_eptp,hpa,cache_generation,history,type_epoch,nonce;};
struct td_ticket {uint64_t domain,incarnation,nonce,version,type_epoch;};
struct td_snapshot {
 struct td_identity identity; uint64_t full_eptp,hpa,cache_generation,history;
 uint64_t pins,refills; enum td_state state; bool attached,receipt;
};
struct typed_directory {
 uint64_t domain,incarnation,participants,version,type_epoch,next_id,next_nonce;
 uint64_t operations,freeze_nonce; bool closed,sealed,poisoned;
 struct td_record *records; struct td_receipt *receipts; struct td_operation *ops;
};
bool td_init(struct typed_directory *,uint64_t domain,uint64_t incarnation,uint64_t participants,uint64_t type_epoch);
bool td_begin(struct typed_directory *,struct td_operation *,uint64_t *nonce);
bool td_end(struct typed_directory *,struct td_operation *,uint64_t nonce);
bool td_reserve(struct typed_directory *,struct td_operation *,uint64_t nonce,
 struct td_record *,uint64_t owner,uint64_t allocator_lifetime,struct td_identity *);
/* Explicit initial history must be zero: initialization flush is not entry.
 * eptp is an opaque complete hardware key with root matching hpa. */
bool td_attach(struct typed_directory *,struct td_operation *,uint64_t nonce,
 struct td_record *,struct td_identity,uint64_t eptp,uint64_t hpa,uint64_t cache_generation,uint64_t initial_history);
bool td_publish(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
bool td_retain(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
/* Only a never-attached candidate may cancel without a retirement receipt. */
bool td_cancel(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
bool td_pin(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
bool td_unpin(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
/* Refill registration remains until all corresponding software plans are gone. */
bool td_refill_add(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
bool td_refill_drop(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
bool td_resident(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity,unsigned cpu);
bool td_cache_reset(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity,uint64_t new_generation);
bool td_retire_begin(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity,struct td_retire_ticket *);
/* Read-only, serialized checks for an admitted unlocked retirement attempt
 * and its durable receipt. Neither check executes or authenticates a flush. */
bool td_retire_pending(struct typed_directory *,struct td_operation *,uint64_t nonce,
 struct td_record *,struct td_identity,struct td_retire_ticket);
bool td_receipt_matches(const struct typed_directory *,const struct td_receipt *,struct td_retire_ticket);
/* Caller attests actual flush of ALL historical consumers after no VMCS or
 * refill source can publish. Receipt storage must outlive record/tree free.
 * Failure does not authorize free; RETIRING must remain retained. */
bool td_retire_commit(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity,
 struct td_retire_ticket,uint64_t acked,uint64_t flush_serial,struct td_receipt *);
bool td_forget(struct typed_directory *,struct td_operation *,uint64_t nonce,struct td_record *,struct td_identity);
/* close rejects unfinished retirements without changing the gate. Once
 * closed, no new retirement ticket can start, including inside admitted work.
 * close rejects new operations but lets already admitted operations finish.
 * seal only after drain; snapshots cover attached/unattached retained records
 * and durable receipts. No mutation is admitted while sealed. */
bool td_close(struct typed_directory *);
bool td_seal(struct typed_directory *,struct td_ticket *);
bool td_ticket_valid(const struct typed_directory *,struct td_ticket);
bool td_snapshot_read(const struct typed_directory *,struct td_ticket,struct td_snapshot *,size_t capacity,size_t *count);
/* Metadata only: caller completed all transition obligations separately.
 * new_epoch must strictly advance; reopen invalidates the freeze ticket. */
bool td_reopen(struct typed_directory *,struct td_ticket,uint64_t new_epoch);
void td_poison(struct typed_directory *);
#endif
