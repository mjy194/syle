#ifndef SYLE_TYPED_ROOT_LOCAL_PROVIDER_H
#define SYLE_TYPED_ROOT_LOCAL_PROVIDER_H
#include "typed_root_registry.h"

/* N39B1: owner-local hardware provider, shared unchanged with host tests.
 * All hooks are trusted platform code. current_cpu must observe the physical
 * CPU, not return a context's desired owner. invalidate executes one actual
 * single-context INVEPT of the supplied full EPTP and reports that execution.
 * No remote dispatch/ACK is supported. The prior guarded-destroy flush is
 * deliberately insufficient: this provider performs a separately bound flush.
 * One writer per owner; shared metadata, serial and stable storage use lock.
 * Hooks do not allocate. invalidate is called without the metadata lock. */
struct tr_local_invalidation {
 uint64_t full_eptp,acked;
 unsigned cpu;
 bool success;
};
struct tr_local_completed {
 struct tr_registry_retire_scope scope;
 struct tr_local_invalidation invalidation;
 uint64_t acked,flush_serial;
 bool complete;
};
struct tr_local_provider {
 struct typed_root_registry *bridge;
 struct tr_registry_slot *slots;
 struct tr_local_completed *completed;
 size_t slot_count;
 unsigned owner;
 uint64_t *serial;
 bool (*lock)(void *);
 void (*unlock)(void *);
 unsigned (*current_cpu)(void *);
 bool (*healthy)(void *);
 bool (*invalidate)(void *,uint64_t,struct tr_local_invalidation *);
 void *context;
};
/* Caller supplies preallocated, non-recycled slots and matching completed
 * evidence storage. Publication is under lock only after full revalidation.
 * Output and completed evidence are unchanged on rejection. The registry
 * bridge must independently revalidate and commit its own durable receipt.
 * Neither this function nor a retired root authorizes backing release. */
bool tr_local_provider_flush(struct tr_local_provider *,struct tr_registry_slot *,
 struct tr_registry_retire_scope,struct tr_registry_root_evidence *);
/* Stable final audit under the caller's metadata lock. Does not dereference
 * the freed registry item/tree, and does not execute or infer a new flush. */
bool tr_local_completed_matches(const struct tr_local_provider *,const struct tr_registry_slot *);
#endif
