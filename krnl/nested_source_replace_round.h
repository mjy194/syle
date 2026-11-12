#ifndef SYLE_NESTED_SOURCE_REPLACE_ROUND_H
#define SYLE_NESTED_SOURCE_REPLACE_ROUND_H
#include "nested_owner_source_replace.h"
#define NSRR_POLLS 268435456u
enum nsrr_command {NSRR_NONE,NSRR_RETIRE,NSRR_PREPARE,NSRR_VALIDATE};
/* Two fixed cooperative owners, one round. After arrival each owner remains
 * inside this command loop and must not touch ordinary registry/config APIs.
 * Exact tsr_released, not domain OPEN or a second flag, is the only release. */
struct nested_source_replace_round {
 struct tr_source_replace replacement;
 struct nested_owner_source_replace refs[2];
 struct nested_owner_atomic *owners[2];
 struct nested_domain_dispatch *dispatch;
 tsr_change_fn change;void *change_context;
 bool (*healthy)(void *);void (*fault)(void *);void *context;
 void (*metadata_lock)(void *),(*metadata_unlock)(void *);void *metadata_context;
 /* Optional immutable N48 authority. Checked before all ordinary reads.
  * chain_fault is set under the same metadata lock as successor publication. */
 uint64_t *active_generation,*chain_fault,generation;
 struct tsr_ticket ticket;struct tsr_receipt validated[2];
 uint32_t empty[2],bootstrapped,arrived[2],command,ack[2],parked[2],committed[2];
};
bool nsrr_empty_ready(struct nested_owner_atomic *,void *);
/* Caller has actually completed full physical_leave, exit reflection and
 * nvmx_return_from_l2; current VMCS01, software in_l2 false, model not running. */
bool nsrr_run(struct nested_source_replace_round *,unsigned);
/* Must be called BEFORE ordinary object reads on every owner execution loop. */
bool nsrr_before_entry(struct nested_source_replace_round *,unsigned);
/* Internal controller helpers. successor init requires shared metadata lock;
 * retain_stop fences stale generations before any global fault callback. */
bool nsrr_init_successor_locked(struct nested_source_replace_round *,struct tsr_successor_handoff *);
bool nsrr_retain_stop(struct nested_source_replace_round *);
#endif
