#ifndef SYLE_TYPED_ROOT_SOURCE_REPLACE_H
#define SYLE_TYPED_ROOT_SOURCE_REPLACE_H
#include "typed_root_atomic.h"
#include "typed_root_local_provider.h"
#include "nested_domain.h"
#define TSR_OWNERS 2u
#define TSR_ROOTS 8u
#define TSR_DRAIN_POLLS 1048576u
/* One retained WB4K/IPAT1/AD1 replacement, two fixed owners. Fresh coordinator,
 * two stable banks. No source/data freeing or arbitrary dynamic EPT API.
 * Both configurations use the same external short metadata lock. One writer
 * per owner and one coordinator caller, except abort may race every stage.
 * Init is exclusive. Cross-owner mutable inventory is read only after drain;
 * release polling reads only synchronized shared health, not peer-local state. */
struct tsr_ticket {struct nrc_identity source;struct ad_entry_stamp stamp;uint64_t nonce;struct nested_domain_maintenance_ticket domain;};
/* Stable slot plus copied values; NEVER a retired item pointer. */
struct tsr_root {struct tr_registry_slot *slot;struct td_identity identity;struct nrc_root source;uint64_t key,eptp,hpa,generation,history;uint32_t pins;};
struct tsr_receipt {struct tsr_ticket ticket;struct tsr_root root;unsigned owner;bool success;};
struct tsr_hooks {
 /* Caller already reached full cooperative checkpoints. Close only after both
  * actual consumers left, without prepared/running/internal pending state. */
 /* Read-only checkpoint/preflight: returns the expected next exact domain
  * ticket, without closing the physical gate. A single control caller owns
  * this interval. The coordinator binds it before invoking close. */
 bool (*plan_close)(void *,struct tsr_ticket,struct nested_domain_maintenance_ticket *);
 bool (*close)(void *,struct tsr_ticket);
 bool (*stopped)(void *,unsigned,struct tsr_ticket);
 /* Audits ALL actual VMCS references and unchanged launch state; exact park
  * write/readback must finish before success. False preserves old pins. */
 bool (*park)(void *,unsigned,struct tsr_ticket,const struct tsr_root *,size_t,struct tsr_receipt *);
 /* New pin is already held. Restore writes/reads actual EPTP and verifies
  * launch state; uncertainty retains the new pin, never rolls back. */
 bool (*restore)(void *,unsigned,struct tsr_ticket,const struct tsr_root *,struct tsr_receipt *);
 /* Revalidate both owner-local actual VMCS EPTPs/launch state against their
  * completed restore receipts, then domain ack/publish/complete, all outside
  * the metadata lock. Normal admission stays
  * shut until return is checked under the SAME lock as abort. */
 bool (*complete)(void *,struct tsr_ticket);
};
enum tsr_state {TSR_FRESH,TSR_OPEN,TSR_STOPPING,TSR_BOUND,TSR_DRAINING,TSR_UPDATER_DRAIN,TSR_CLOSED,TSR_RETIRED,TSR_CHANGING,TSR_PUBLISHED,TSR_COMMITTING,TSR_RELEASED,TSR_POISONED};
enum tsr_drain {TSR_DRAIN_FAILED,TSR_DRAIN_WAIT,TSR_DRAIN_READY};
struct tsr_command {uint64_t nonce,key;enum nested_context_operation kind;struct tsr_root root;bool armed,consumed,admitted;};
struct tsr_successor_handoff;
struct tr_source_replace {
 struct tr_atomic_owner *owners[TSR_OWNERS];
 struct tr_atomic_config *mirrors[TSR_OWNERS]; /* optional wrapper.config */
 struct tr_local_provider *providers[TSR_OWNERS];
 struct tsr_hooks hooks;void *context;enum tsr_state state;
 struct tsr_ticket ticket;struct ad_entry_stamp new_stamp;uint64_t next_nonce,next_command,type_epoch,retired,prepared;
 unsigned polls;bool busy[TSR_OWNERS];
 struct nested_ept_contexts registry_config[TSR_OWNERS];
 struct tsr_root old[TSR_OWNERS][TSR_ROOTS],fresh[TSR_OWNERS];size_t count[TSR_OWNERS];
 uint64_t active_key[TSR_OWNERS];struct tsr_receipt park[TSR_OWNERS],restore[TSR_OWNERS];
 struct tsr_command command[TSR_OWNERS];
 struct nested_root_catalog *old_catalog,*new_catalog;struct ad_updater_gate *old_gate,*new_gate;
 /* N48: reserved before first close, never a reset of released evidence. */
 struct tsr_successor_handoff *successor;
 const struct tr_source_replace *predecessor;
 uint64_t generation;
};
struct nested_owner_source_replace;
/* Stable controller-owned storage. Bind once before the first close. Owner
 * publication is after nsrr_run RETURNS, not merely after nosr_commit. The
 * enclosing controller also keeps the next checkpoint parked until publication.
 * Every API below is called with the common metadata lock already held; no
 * platform callback or allocator is invoked. Physical checks occur outside it.
 * departed/arrived are generation tags, never resettable Boolean ACKs. */
struct tsr_successor_handoff {
 struct tsr_successor_handoff *self;
 struct tr_source_replace *prior,*next;
 struct nested_owner_source_replace *refs[TSR_OWNERS];
 uint64_t *active_generation,prior_generation,next_generation;
 struct tsr_ticket ticket;
 struct tsr_receipt departure[TSR_OWNERS],checkpoint[TSR_OWNERS];
 uint64_t departed[TSR_OWNERS],arrived[TSR_OWNERS];
 bool published;
};
bool tsr_successor_storage_locked(struct tr_source_replace *,const void *,size_t);
bool tsr_bind_successor_locked(struct tsr_successor_handoff *,struct tr_source_replace *,
 struct tr_source_replace *,struct nested_owner_source_replace *,struct nested_owner_source_replace *,uint64_t *,uint64_t);
bool tsr_departed_locked(struct tsr_successor_handoff *,unsigned,struct tsr_ticket);
bool tsr_checkpoint_locked(struct tsr_successor_handoff *,unsigned,uint64_t,struct tsr_receipt);
/* Exact registered handoff; initializes only its reserved fresh successor.
 * owner.replacement and active generation publish in this same critical
 * section. Old coordinator/ticket/receipts remain unchanged. Root source pins
 * remain live until the next round retires them; nrc_idle is NOT required. */
bool tsr_init_successor_locked(struct tsr_successor_handoff *,const struct tsr_hooks *,void *);
bool tsr_init(struct tr_source_replace *,struct tr_atomic_owner *,struct tr_atomic_owner *,
 struct tr_local_provider *,struct tr_local_provider *,struct tr_atomic_config *,struct tr_atomic_config *,const struct tsr_hooks *,void *);
bool tsr_close(struct tr_source_replace *,struct tsr_ticket *);
enum tsr_drain tsr_drain(struct tr_source_replace *,struct tsr_ticket);
bool tsr_retire_owner(struct tr_source_replace *,unsigned,struct tsr_ticket);
/* Callback is exclusive management authority: one exact CAS, complete retained
 * source audit, successor banks and publication. It must not reenter tsr.
 * Existing ordinary source resolvers never grant this authority. */
typedef bool (*tsr_change_fn)(void *,struct tsr_ticket,struct nested_root_catalog *,struct ad_updater_gate *,struct nested_root_catalog **,struct ad_updater_gate **,void *new_authority_context[TSR_OWNERS]);
bool tsr_replace(struct tr_source_replace *,struct tsr_ticket,tsr_change_fn,void *);
bool tsr_prepare_owner(struct tr_source_replace *,unsigned,struct tsr_ticket);
bool tsr_finish(struct tr_source_replace *,struct tsr_ticket);
/* Exact ticket; a stale abort AFTER successful release cannot revoke a later
 * completed result. If abort wins, no subsequent finish may publish release. */
bool tsr_abort(struct tr_source_replace *,struct tsr_ticket);
bool tsr_released(struct tr_source_replace *,struct tsr_ticket);
/* Production stopped routing for admitted drain/cleanup, not normal admission. */
bool tsr_consumer_stopped(struct tr_atomic_owner *);
#endif
