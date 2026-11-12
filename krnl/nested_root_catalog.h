#ifndef SYLE_NESTED_ROOT_CATALOG_H
#define SYLE_NESTED_ROOT_CATALOG_H
#include "nested_ept.h"
#include <stddef.h>
#define NRC_LOCK_TRIES 1048576u
#define NRC_TYPE_WB (1u<<6)
#define NRC_TYPE_UC 1u
/* N40A: trusted L0-owned, guest-read-only source catalog. AD0, WB EPTP,
 * four levels/4K leaves only. No guest structural writer, source replacement,
 * catalog reopen or backing-release authority is implemented. */
struct nrc_identity { uint64_t domain,incarnation,catalog,lifetime; };
enum nrc_role { NRC_DATA=1,NRC_TABLE=2 };
struct nrc_span {
 struct nrc_identity identity;
 uint64_t object,lifetime,gpa,hpa,bytes;
 const uint64_t *table_view; /* NULL for DATA; TABLE only, entire span */
 uint8_t permissions,allowed_types;
 enum nrc_role role;
 bool ignore_pat;
};
struct nrc_root {
 struct nrc_identity identity;
 uint64_t source,owner,generation,lifetime,full_eptp,root_gpa;
};
/* Returned by trusted authority, never by a guest HPA. Every field is matched
 * against the catalog. Authority must prove current ownership/lifetime, all
 * guest aliases, actual EPT01 permissions/type and a valid entire host page.
 * A host pointer or callback success alone is NOT an ownership/alias proof.
 * The caller keeps backing and immutable structure pinned through each lease
 * and all cache/physical consumers. Mutable type epoch is separate from the
 * source structural generation; external stopped-domain maintenance owns it. */
struct nrc_page {
 uint64_t object,lifetime,hpa;
 const uint64_t *table_view;
 uint8_t permissions,memory_type;
 bool ignore_pat;
};
typedef bool (*nrc_authority_fn)(void *,const struct nrc_span *,uint64_t,struct nrc_page *);
/* Atomic authority. TABLE success attests owned, pinned WB writable host
 * RAM, immutable structural fields, and ALL guest aliases read-only. DATA
 * returns its exact audited current type (WB, or the N46 fixed UC target).
 * Every reader/writer of these shared words, including another catalog or
 * diagnostics, must use 64-bit atomics. No ordinary AD0 alias may coexist.
 * These global obligations are trusted prepublication proof, not inferred
 * from a pointer or Boolean. atomic_view is independent L0 write authority. */
struct nrc_atomic_page {
 struct nrc_page backing;
 uint64_t *atomic_view;
 bool pinned_writable;
};
typedef bool (*nrc_atomic_authority_fn)(void *,const struct nrc_span *,uint64_t,struct nrc_atomic_page *);
struct nested_root_catalog;
struct nrc_session {
 struct nested_root_catalog *catalog;
 struct nrc_session *next;
 struct nrc_root root;
 void *authority_context;
 uint64_t nonce;
 bool live;
};
struct nested_root_catalog {
 /* Immutable construction provenance; NULL for ordinary constructors. */
 const struct nested_root_catalog *predecessor;
 struct nrc_identity identity;
 struct nrc_span *spans;
 struct nrc_root *roots;
 size_t span_count,span_capacity,root_count,root_capacity;
 nrc_authority_fn authority;
 unsigned maxphys;
 uint32_t lock,fault;
 uint64_t references,next_nonce;
 struct nrc_session *sessions;
 bool initialized,published,closed,superseded;
 bool atomic_wb; /* AD1, WB table walk; legacy constructor remains WB leaves only. */
 bool atomic_uc; /* separate fixed dedicated UC target; never live retyped */
 uint64_t uc_gpa,uc_hpa,type_epoch; /* immutable UC epoch, independent of AD */
 bool atomic_2m,atomic_1g; /* immutable constructor policy; never a live source update */
 nrc_atomic_authority_fn atomic_authority;
};
/* Fresh stable storage only. Identity/lifetime values are never reused.
 * Caller exclusively builds, then publishes before sharing. Storage and all
 * registered records belong exclusively to the catalog immediately after
 * registration; callers never edit them directly, even before publication.
 * Records are immutable after publication, except synchronized gate metadata.
 * Rejection leaves the object/output unchanged. No allocation/fallback.
 * Root and span registration are TRUSTED host management, not guest requests.
 * No GPA or HPA overlap is supported, even a compatible read-only alias. */
bool nrc_init(struct nested_root_catalog *,struct nrc_identity,unsigned,
 struct nrc_span *,size_t,struct nrc_root *,size_t,nrc_authority_fn);
bool nrc_add_span(struct nested_root_catalog *,struct nrc_span);
/* New separate contract: AD1-only exact roots and WB/IPAT1 backing. Ordinary
 * add_span/resolve/walk reject this mode. No mixed AD0/AD1 catalog or retype.
 * TABLE registration requires its explicit writable view, DATA requires NULL.
 * Neither constructor audits physical aliases or pins memory by itself. */
bool nrc_init_atomic_wb(struct nested_root_catalog *,struct nrc_identity,unsigned,
 struct nrc_span *,size_t,struct nrc_root *,size_t,nrc_atomic_authority_fn);
/* N47A: consume one closed, drained WB4K predecessor. new storage must be
 * zero-initialized, exclusive and disjoint from old metadata/records; arrays
 * remain caller-owned fresh storage. Same domain/incarnation, fresh catalog
 * and lifetime. Capacity must retain at least the old record counts. A nonzero
 * nonce budget is checked under old's lock; each domain inherits its own final
 * highwater before publication. Success permanently supersedes old. Failure
 * leaves new and old unchanged (except ordinary lock-exhaustion poison).
 * Does not retire backing, invalidate consumers, or publish the successor. */
bool nrc_init_atomic_wb_successor(struct nested_root_catalog *new_catalog,
 struct nested_root_catalog *old_catalog,struct nrc_identity old_id,
 struct nrc_identity new_id,unsigned,struct nrc_span *,size_t,
 struct nrc_root *,size_t,nrc_atomic_authority_fn,uint64_t reserve_nonces);
/* N44 opt-in permits immutable 2 MiB EPT12 leaves. Per-4K registered spans
 * still authorize backing independently; no whole-2MiB coverage is required.
 * No 1GiB, writable source aliases, structural updates or reclamation. */
bool nrc_init_atomic_wb_2m(struct nested_root_catalog *,struct nrc_identity,unsigned,
 struct nrc_span *,size_t,struct nrc_root *,size_t,nrc_atomic_authority_fn);
/* N45 separate opt-in adds immutable 1 GiB PDPTE leaves to 4K/2M support.
 * Backing remains sparse per-4KiB authority, with the same retention, alias,
 * type and lease contract. Does not add mutable source or reclaim support. */
bool nrc_init_atomic_wb_1g(struct nested_root_catalog *,struct nrc_identity,unsigned,
 struct nrc_span *,size_t,struct nrc_root *,size_t,nrc_atomic_authority_fn);
/* N46A fixed one-page UC data policy. Dedicated UC target is exact and
 * nonexecuting; all other DATA and every TABLE span stay singleton WB.
 * No type union, live transition, source mutation, huge leaves or release.
 * Caller establishes and audits the physical UC mapping BEFORE publication. */
bool nrc_init_atomic_uc(struct nested_root_catalog *,struct nrc_identity,unsigned,
 struct nrc_span *,size_t,struct nrc_root *,size_t,nrc_atomic_authority_fn,
 uint64_t target_gpa,uint64_t target_hpa,uint64_t type_epoch);
bool nrc_add_atomic_span(struct nested_root_catalog *,struct nrc_span,uint64_t *);
/* Lower-level authority resolver, not updater admission by itself. Caller
 * must also hold a valid updater permit whenever accessing shared words;
 * nested_atomic_catalog.h composes the source and updater lifetimes. */
bool nrc_resolve_atomic(struct nrc_session *,uint64_t,uint64_t,uint8_t,struct nrc_atomic_page *);
bool nrc_add_root(struct nested_root_catalog *,struct nrc_root);
bool nrc_publish(struct nested_root_catalog *);
/* Exact full EPTP membership, never canonical-address-only matching. */
bool nrc_find(struct nested_root_catalog *,struct nrc_identity,uint64_t,struct nrc_root *);
/* Stable, exclusive caller session storage must survive failure, including
 * failed end. Keep the saved nonce separately; reading session.nonce afresh
 * cannot defeat same-storage ABA. Metadata locks never span callbacks/walks.
 * Close rejects new work but admitted sessions may finish; drain is not proof
 * that hardware/cached translations are gone and never authorizes page free. */
bool nrc_begin(struct nested_root_catalog *,struct nrc_root,void *,struct nrc_session *,uint64_t *);
bool nrc_guard(struct nrc_session *,uint64_t);
bool nrc_end(struct nrc_session *,uint64_t);
bool nrc_close(struct nested_root_catalog *,struct nrc_identity);
bool nrc_drained(struct nested_root_catalog *,struct nrc_identity);
/* N48B stopped management snapshot, not a reservation or mutation license.
 * Exact current bank must be closed/drained/nonterminal. Output unchanged on
 * failure. Existing bounded-lock exhaustion may latch fault. Caller
 * independently excludes all consumers through later CAS. */
bool nrc_successor_budget(struct nested_root_catalog *,struct nrc_identity,uint64_t reserve,uint64_t *nonce);
/* Open-gate snapshot only. A caller must independently stop all consumers and
 * asynchronous sources before using idle as maintenance evidence. */
bool nrc_idle(struct nested_root_catalog *,struct nrc_identity);
/* Need is a nonzero access mask. Returns actual permissions even if denied;
 * the walker decides fault status. Outputs remain unchanged on rejection. */
bool nrc_resolve(struct nrc_session *,uint64_t,uint64_t,uint8_t,struct nrc_page *);
/* Complete path backing denial precedes L1 permission denial; a nonpresent
 * terminal is L1_DENIED without interpreting its ignored payload. Unsupported
 * format/API/source identity never becomes architectural reason49. No install.
 * Caller must check status==OK and guard again before external publication. */
struct nested_ept_result nrc_walk(struct nrc_session *,uint64_t,uint64_t,uint8_t);
#endif
