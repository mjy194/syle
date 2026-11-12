#ifndef SYLE_TYPED_ROOT_ATOMIC_MAINTENANCE_H
#define SYLE_TYPED_ROOT_ATOMIC_MAINTENANCE_H
#include "typed_root_atomic.h"
/* N42A: bounded fixed-owner AD-only maintenance. No VMX or backing release.
 * Stable coordinator/owners, one control caller and one writer per owner.
 * All shared metadata uses owners[0]'s existing directory lock. The two owner
 * configurations MUST refer to that same external lock, even if contexts differ.
 * Software admission closes while reserving the round; trusted adapters then
 * stop/close physical admission before the round becomes drainable, keeping all
 * physical consumers stopped until open_entry. Every failure retains resources.
 * Callbacks may probe normal admission, but must not recursively call
 * maintenance APIs or mutate coordinator/bridge/context metadata.
 * Wrong-ticket/out-of-order/duplicate calls reject without mutation. A valid
 * started action failing its checks poisons and retains; no rollback. */
#define TAM_OWNERS 2u
#define TAM_ROOTS 8u
#define TAM_PAGES 16u
#define TAM_DRAIN_POLLS 1048576u
struct tam_ticket {struct ad_entry_stamp stamp;uint64_t nonce;};
struct tam_root {
 struct tr_atomic_binding *binding;
 struct nested_ept_context_item *item;
 struct td_identity identity;
 struct nrc_root source;
 uint64_t full_eptp,hpa,history,old_generation,new_generation;
};
struct tam_flush_receipt {
 struct tam_ticket ticket;
 struct td_identity identity;
 uint64_t full_eptp,hpa,history,old_generation,new_generation,acked;
 unsigned cpu;
 bool success;
};
struct tam_hooks {
 bool (*close_entry)(void *,struct tam_ticket);
 bool (*stopped)(void *,unsigned,struct tam_ticket);
 /* Must inspect all actual owner VMCS references, including inactive roots.
  * Actual private-root references must be absent on success; metadata pins are released
  * only afterwards. False/uncertain outcomes retain every original pin. */
 bool (*remove_refs)(void *,unsigned,struct tam_ticket);
 /* Extra explicit invalidation during guarded sparse reset, BEFORE lower-page
  * free or generation advance. Receipt must identify
  * the operation, complete root identity, generation pair and exact CPU. */
 bool (*flush)(void *,unsigned,struct tam_ticket,const struct tam_root *,struct tam_flush_receipt *);
 /* Called AFTER the active private root is re-pinned. Failure keeps that pin.
  * Must restore the actual full EPTP without resetting launch bookkeeping. */
 bool (*restore_refs)(void *,unsigned,struct tam_ticket,const struct tam_root *);
 bool (*open_entry)(void *,struct tam_ticket);
 /* Paired optional linearizable test backend. CAS false writes nothing.
  * NULL selects real acquire-load/acq_rel-CAS. Never ordinary table access. */
 uint64_t (*load)(void *,uint64_t *);
 bool (*cas)(void *,uint64_t *,uint64_t,uint64_t);
};
enum tam_state { TAM_UNINITIALIZED,TAM_OPEN,TAM_STOPPING,TAM_CLOSING,TAM_UPDATER_DRAIN,TAM_DRAINED,TAM_CLEARING,TAM_CLEARED,TAM_PREPARING,TAM_REOPENING,TAM_POISONED };
enum tam_drain_result { TAM_DRAIN_FAILED,TAM_DRAIN_WAIT,TAM_DRAIN_READY };
struct tam_page {
 uint64_t gpa,hpa,*view,raw[512];
 unsigned depth;
 bool visited;
};
struct tr_atomic_maintenance {
 struct tr_atomic_owner *owners[TAM_OWNERS];
 struct tam_hooks hooks;void *context;
 struct tam_ticket ticket;
 struct ad_entry_stamp next_stamp;
 enum tam_state state;
 uint64_t next_nonce,type_epoch,revoked,reset,prepared,changed,cleared_entries;
 unsigned drain_polls;
 struct tam_root roots[TAM_OWNERS][TAM_ROOTS];
 struct tam_flush_receipt receipts[TAM_OWNERS][TAM_ROOTS];
 size_t root_count[TAM_OWNERS];
 struct tam_page pages[TAM_PAGES];size_t page_count;
 struct nrc_root sources[TAM_PAGES];size_t source_count;
 struct td_operation operations[TAM_OWNERS];uint64_t operation_nonce[TAM_OWNERS];
 bool owner_busy[TAM_OWNERS];
};
/* Fresh initialized empty owners; binds future normal context/fault/entry
 * admissions to this coordinator. No catalog/type identity change. */
bool tam_init(struct tr_atomic_maintenance *,struct tr_atomic_owner *,struct tr_atomic_owner *,const struct tam_hooks *,void *);
bool tam_close(struct tr_atomic_maintenance *,struct tam_ticket *);
/* Bounded polling. WAIT is not an error; exceeding TAM_DRAIN_POLLS poisons.
 * Neither persistent catalog pins nor nrc_idle are used as updater drain. */
enum tam_drain_result tam_drain(struct tr_atomic_maintenance *,struct tam_ticket);
bool tam_revoke(struct tr_atomic_maintenance *,unsigned,struct tam_ticket);
bool tam_clear(struct tr_atomic_maintenance *,struct tam_ticket);
bool tam_reset(struct tr_atomic_maintenance *,unsigned,struct tam_ticket);
bool tam_prepare(struct tr_atomic_maintenance *,unsigned,struct tam_ticket);
bool tam_finish(struct tr_atomic_maintenance *,struct tam_ticket);
/* Any adapter timeout/uncertain hardware result must abort, never rollback. */
void tam_abort(struct tr_atomic_maintenance *);
#endif
